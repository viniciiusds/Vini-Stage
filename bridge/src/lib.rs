use nord_format::formats::npno::{codec, Piano};
use std::{ffi::{CStr, c_char, c_void}, fs::File, io::{BufReader, Read, Seek, SeekFrom}, panic::{catch_unwind, AssertUnwindSafe}};

type CancelCallback=Option<unsafe extern "C" fn(*mut c_void)->bool>;
type ProgressCallback=Option<unsafe extern "C" fn(*mut c_void,u32)>;
struct Progress { cancelled:CancelCallback, callback:ProgressCallback, context:*mut c_void, last:Option<u32> }
impl Progress {
    fn check(&self)->std::io::Result<()>{if self.cancelled.is_some_and(|f|unsafe{f(self.context)}){Err(std::io::Error::new(std::io::ErrorKind::Other,"Load cancelled"))}else{Ok(())}}
    fn report(&mut self,value:u32){
        let value=value.min(100);
        if self.last.map_or(true,|last|value>last){
            self.last=Some(value);
            if let Some(callback)=self.callback {unsafe{callback(self.context,value);}}
        }
    }
}
struct ProgressReader<'a>{inner:BufReader<File>,position:u64,furthest:u64,total:u64,progress:&'a mut Progress}
impl Read for ProgressReader<'_>{
    fn read(&mut self,buffer:&mut [u8])->std::io::Result<usize>{
        self.progress.check()?;
        let count=self.inner.read(buffer)?;
        self.position+=count as u64;self.furthest=self.furthest.max(self.position);
        self.progress.report((self.furthest.min(self.total)*20/self.total.max(1)) as u32);
        Ok(count)
    }
}
impl Seek for ProgressReader<'_>{
    fn seek(&mut self,from:SeekFrom)->std::io::Result<u64>{self.position=self.inner.seek(from)?;Ok(self.position)}
}

struct Recording { root: u8, bank: u8, layer: u8, channels: u16, frames: usize, pcm: Vec<i16>, loop_start:usize, loop_end:usize, loop_decay:f32, attack_decay:f32, sample_gain:f32, release_ms:f32, boundary:u16, peak:f32 }
// Vendor records locate a repeat using a start frame plus its length. The next
// marks bracket the decoder block containing the start. Reject inconsistent data.
fn loop_info(record:&[u8;118],bank:u8,frames:usize)->(usize,usize,f32){
    let word=|at|u32::from_be_bytes(record[at..at+4].try_into().unwrap()) as usize;
    let start=word(0x1c);let length=word(0x20);let block_start=word(0x24);let block_end=word(0x28);
    let end=start.checked_add(length).unwrap_or(usize::MAX);
    if bank>1||start==0||length<64||end>frames||block_start>start||start>=block_end||block_end>frames{return (0,0,1.0);}
    let decay=word(0x2e) as f64 / 8_388_608.0;
    (start,end,if decay>0.0&&decay<=1.0{decay as f32}else{1.0})
}
// Independent implementation of the observed metadata units, not recovered source.
fn rate_from_q23(bytes:&[u8])->Option<f64>{
    let q=u32::from_be_bytes(bytes.try_into().ok()?);
    if q==0||q>8_388_608{return None;}
    Some((-20.0*(q as f64/8_388_608.0).log10()*21680.0).clamp(0.0,60.0))
}
fn measured_decay(pcm:&[i16],channels:u16,loop_start:usize,root:u8)->f64{
    let fallback=60.0/(18.0*2f64.powf((60.0-root as f64)/36.0)).clamp(3.0,40.0);
    let window=(codec::RATE as usize)/10;
    if loop_start<window*3{return fallback;}
    let start=loop_start.saturating_sub(window*15).max(window*2);
    let mut points=Vec::new();
    for at in (start..loop_start.saturating_sub(window)).step_by(window){
        let end=((at+window)*channels as usize).min(pcm.len());let begin=at*channels as usize;
        if begin>=end{break;}
        let power=pcm[begin..end].iter().map(|&v|(v as f64).powi(2)).sum::<f64>()/(end-begin)as f64;
        if power>1.0{points.push((at as f64/codec::RATE as f64,10.0*power.log10()));}
    }
    if points.len()<3{return fallback;}
    let n=points.len()as f64;let sx=points.iter().map(|p|p.0).sum::<f64>();let sy=points.iter().map(|p|p.1).sum::<f64>();
    let denom=n*points.iter().map(|p|p.0*p.0).sum::<f64>()-sx*sx;
    if denom<=0.0{return fallback;}
    let slope=-(n*points.iter().map(|p|p.0*p.1).sum::<f64>()-sx*sy)/denom;
    if slope.is_finite(){slope.clamp(fallback,40.0)}else{fallback}
}
fn metadata(record:&[u8;118],root:u8,bank:u8,pcm:&[i16],channels:u16,loop_start:usize,version:u16)->(f32,f32,f32,f32){
    let fallback=||measured_decay(pcm,channels,loop_start,root);
    let attack_db=rate_from_q23(&record[0x2e..0x32]).unwrap_or_else(fallback);
    let loop_db=rate_from_q23(&record[0x3a..0x3e]).unwrap_or(0.0);
    let coefficient=|db:f64|10f64.powf(-db/(20.0*codec::RATE as f64))as f32;
    let mut trim=i16::from_be_bytes(record[0x34..0x36].try_into().unwrap())as i32;
    if version==0 {trim=(trim-i16::from_be_bytes(record[0x32..0x34].try_into().unwrap())as i32)*10;}
    let gain=if bank==2 {1.0}else if (-600..=600).contains(&trim){1.5*10f32.powf(-(trim as f32)/200.0)}else{
        let coefficient=(-std::f64::consts::TAU*3000.0/codec::RATE as f64).exp();let mut states=[0.0f64;2];let mut peak=1.0f64;for (i,&v) in pcm.iter().enumerate(){let ch=i%channels as usize;states[ch]=coefficient*states[ch]+(1.0-coefficient)*v as f64;peak=peak.max(states[ch].abs());}0.65*32768.0/peak as f32
    };
    let release_ms=if record[0x72]==0{500.0}else{(2.0*2f32.powf(record[0x72]as f32/6.0)).clamp(10.0,4000.0)};
    (coefficient(attack_db),coefficient(loop_db),gain,release_ms)
}
pub struct Library { name: std::ffi::CString, keys: [u8; 128], fine: [i8; 128], key_gain:[f32;128], recordings: Vec<Recording>, gain: f32, damper: u8 }
fn load(path: &str,progress:&mut Progress) -> Result<Library, Box<dyn std::error::Error>> {
    let clock=std::time::Instant::now();let mut decode_time=0.0;let mut prepare_time=0.0;
    let file=File::open(path)?;let total=file.metadata()?.len();
    let piano={
        let mut reader=ProgressReader{inner:BufReader::new(file),position:0,furthest:0,total,progress};
        Piano::read_from(&mut reader)?
    };
    progress.report(20);
    let lib = piano.library()?;
    progress.report(25);
    let mut out = Library { name: std::ffi::CString::new(piano.name()?.0)?, keys: [255;128], fine: [0;128], key_gain:[1.0;128], recordings: Vec::new(), gain: 10f32.powf(lib.gain() as f32 / 200.0), damper: lib.damper_top() };
    out.keys.copy_from_slice(lib.key_map());
    for k in 0..128 { out.fine[k] = lib.fine_tune(k as u8)?;out.key_gain[k]=10f32.powf((piano.file.body.0[0x10c+k] as i8)as f32/200.0); }
    let total_audio=lib.strokes().iter().map(|stroke|stroke.audio().len() as u64).sum::<u64>().max(1);
    let read_time=clock.elapsed().as_secs_f64();let mut decoded=0u64;
    for stroke in lib.strokes() {
        progress.check()?;
        let phase=std::time::Instant::now();
        let audio = codec::decode(stroke, lib.channels())?;decode_time+=phase.elapsed().as_secs_f64();let phase=std::time::Instant::now();
        if audio.clipped != 0 { return Err(format!("Audio saturado na nota {}", stroke.root).into()); }
        progress.check()?;
        let (loop_start,loop_end,_)=loop_info(stroke.record(),stroke.bank_code(),audio.frames());
        let pcm=audio.interleaved();
        let meta=metadata(stroke.record(),stroke.root,stroke.bank_code(),&pcm,lib.channels(),loop_start,if matches!(piano.file.header.generation,nord_format::cbin::Generation::V1){1}else{0});
        out.recordings.push(Recording { root: stroke.root, bank: stroke.bank_code(), layer: stroke.layer(), channels: lib.channels(), frames: audio.frames(), loop_start, loop_end, loop_decay:meta.1, attack_decay:meta.0, sample_gain:meta.2, release_ms:meta.3, boundary:stroke.record()[0x74]as u16, peak:if stroke.bank_code()==2{pcm.iter().map(|&v|(v as i32).unsigned_abs()).max().unwrap_or(1).max(1)as f32}else{1.0}, pcm });
        prepare_time+=phase.elapsed().as_secs_f64();
        decoded+=stroke.audio().len() as u64;
        progress.report(25+(decoded*74/total_audio) as u32);
    }
    // Preserve actual spacing: derive boundaries between adjacent recorded layers.
    let scale=if matches!(piano.file.header.generation,nord_format::cbin::Generation::V1){1u16}else{5};
    for root in 0..128 {for bank in 0..3 {
        let mut indices:Vec<usize>=out.recordings.iter().enumerate().filter(|(_,r)|r.root==root&&r.bank==bank).map(|(i,_)|i).collect();
        indices.sort_by_key(|&i|out.recordings[i].layer);
        if indices.iter().all(|&i|out.recordings[i].boundary==0){for (position,&i) in indices.iter().enumerate(){out.recordings[i].boundary=if position==0{0}else{((out.recordings[indices[position-1]].layer as u16+out.recordings[i].layer as u16)*scale+1)/2};}}
        if bank==2 {if let Some(&reference)=indices.last(){
            let peak=|r:&Recording|r.peak;
            let target=peak(&out.recordings[reference]);for &i in &indices {let p=peak(&out.recordings[i]);out.recordings[i].sample_gain*=target/p;}
        }}
    }}
    if !out.recordings.iter().any(|r| r.bank == 0 && r.frames > 0) { return Err("Arquivo sem gravacoes de ataque".into()); }
    progress.check()?;
    if std::env::var_os("VINI_STAGE_PROFILE_LOAD").is_some(){eprintln!("load profile: read={read_time:.3}s decode={decode_time:.3}s prepare={prepare_time:.3}s total={:.3}s",clock.elapsed().as_secs_f64());}
    Ok(out)
}

#[no_mangle]
pub unsafe extern "C" fn np_load(path: *const c_char, error: *mut c_char, cap: usize) -> *mut Library {
    np_load_with_progress(path,error,cap,None,std::ptr::null_mut())
}
#[no_mangle]
pub unsafe extern "C" fn np_load_with_progress(path:*const c_char,error:*mut c_char,cap:usize,callback:ProgressCallback,context:*mut c_void)->*mut Library {
    np_load_cancellable(path,error,cap,callback,None,context)
}
#[no_mangle]
pub unsafe extern "C" fn np_load_cancellable(path:*const c_char,error:*mut c_char,cap:usize,callback:ProgressCallback,cancelled:CancelCallback,context:*mut c_void)->*mut Library {
    let mut progress=Progress{cancelled,callback,context,last:None};
    progress.report(0);
    let result = catch_unwind(AssertUnwindSafe(|| {
        if path.is_null() { return Err("Caminho vazio".to_string()); }
        let path = CStr::from_ptr(path).to_str().map_err(|e| e.to_string())?;
        load(path,&mut progress).map_err(|e| e.to_string())
    }));
    match result {
        Ok(Ok(lib)) => {let pointer=Box::into_raw(Box::new(lib));progress.report(100);pointer},
        failure => {
            let message = match failure { Ok(Err(e)) => e, _ => "Falha ao interpretar o arquivo".into() };
            if !error.is_null() && cap > 0 { let n = message.len().min(cap-1); std::ptr::copy_nonoverlapping(message.as_ptr(), error as *mut u8, n); *error.add(n) = 0; }
            std::ptr::null_mut()
        }
    }
}
#[no_mangle] pub unsafe extern "C" fn np_pcm_bytes(p:*const Library)->usize {(*p).recordings.iter().map(|r|r.pcm.len()*2).sum()}
#[no_mangle] pub unsafe extern "C" fn np_free(p: *mut Library) { if !p.is_null() { drop(Box::from_raw(p)); } }
#[no_mangle] pub unsafe extern "C" fn np_name(p: *const Library) -> *const c_char { (*p).name.as_ptr() }
#[no_mangle] pub unsafe extern "C" fn np_count(p: *const Library) -> usize { (*p).recordings.len() }
#[no_mangle] pub unsafe extern "C" fn np_root(p: *const Library, key: u8) -> u8 { (*p).keys[key as usize] }
#[no_mangle] pub unsafe extern "C" fn np_fine(p: *const Library, key: u8) -> i8 { (*p).fine[key as usize] }
#[no_mangle] pub unsafe extern "C" fn np_key_gain(p:*const Library,key:u8)->f32 {(*p).key_gain[key as usize]}
#[no_mangle] pub unsafe extern "C" fn np_gain(p: *const Library) -> f32 { (*p).gain }
#[no_mangle] pub unsafe extern "C" fn np_damper(p: *const Library) -> u8 { (*p).damper }
#[no_mangle] pub extern "C" fn np_rate() -> u32 { codec::RATE }
#[repr(C)] pub struct View { pub data: *const i16, pub frames: usize, pub channels: u16, pub root: u8, pub bank: u8, pub layer: u8, pub loop_start:usize, pub loop_end:usize, pub loop_decay:f32, pub attack_decay:f32, pub sample_gain:f32, pub release_ms:f32, pub boundary:u16 }
#[no_mangle] pub unsafe extern "C" fn np_recording(p: *const Library, index: usize, out: *mut View) -> bool {
    if p.is_null() || out.is_null() { return false; }
    if let Some(r) = (&(*p).recordings).get(index) { *out = View { data: r.pcm.as_ptr(), frames: r.frames, channels: r.channels, root: r.root, bank: r.bank, layer: r.layer, loop_start:r.loop_start, loop_end:r.loop_end, loop_decay:r.loop_decay, attack_decay:r.attack_decay, sample_gain:r.sample_gain, release_ms:r.release_ms, boundary:r.boundary }; true } else { false }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]fn metadata_units_and_empty_fallback_are_finite(){
        let mut r=record();r[0x3a..0x3e].copy_from_slice(&8_388_500u32.to_be_bytes());r[0x34..0x36].copy_from_slice(&41u16.to_be_bytes());r[0x72]=48;
        let (attack,tail,gain,release)=metadata(&r,60,0,&[0,0],2,1000,1);
        assert!(attack>0.999&&attack<1.0&&tail>0.999&&tail<1.0);
        assert!((gain-1.5*10f32.powf(-4.1/20.0)).abs()<1e-6);
        assert_eq!(release,512.0);
        assert_eq!(rate_from_q23(&0u32.to_be_bytes()),None);
        assert_eq!(rate_from_q23(&8_388_609u32.to_be_bytes()),None);
        assert_eq!(rate_from_q23(&8_388_608u32.to_be_bytes()),Some(0.0));
        let x=metadata(&[0;118],108,0,&[],2,0,1);assert!(x.0.is_finite()&&x.1==1.0&&x.3==500.0);
    }
    #[test]fn cancelled_reader_fails_instead_of_retrying(){
        unsafe extern "C" fn cancel(_: *mut c_void)->bool{true}
        let p=Progress{cancelled:Some(cancel),callback:None,context:std::ptr::null_mut(),last:None};assert!(p.check().is_err());
    }
    fn record()->[u8;118]{
        let mut result=[0;118];
        for (offset,value) in [(0x1c,1000u32),(0x20,2000),(0x24,900),(0x28,1100),(0x2e,8_388_423)]{
            result[offset..offset+4].copy_from_slice(&value.to_be_bytes());
        }result
    }
    #[test]fn loop_uses_start_plus_length_and_q23_decay(){
        let (start,end,decay)=loop_info(&record(),1,3500);
        assert_eq!((start,end),(1000,3000));assert!(decay>0.999&&decay<1.0);
    }
    #[test]fn release_and_malformed_marks_do_not_loop(){
        assert_eq!(loop_info(&record(),2,3500),(0,0,1.0));
        assert_eq!(loop_info(&record(),0,2000),(0,0,1.0));
        assert_eq!(loop_info(&[0;118],0,3500),(0,0,1.0));
        let mut bad=record();bad[0x24..0x28].copy_from_slice(&1001u32.to_be_bytes());
        assert_eq!(loop_info(&bad,0,3500),(0,0,1.0));
    }
}
