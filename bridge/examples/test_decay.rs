//! Independent constant-amplitude sine with known, different pre/post-loop rates.
use nord_format::formats::npno::{codec,encode::{self,Donor,Options,Recording,Rules},Bank};
fn main(){
    let destination=std::env::args().nth(1).expect("Output .npno path");
    let samples=(0..105_000).map(|i|(6000.0*(std::f64::consts::TAU*i as f64/350.0).sin())as i16).collect::<Vec<_>>();
    let recordings=[Recording{root:60,bank:Bank::Attack,layer:0,channels:vec![samples.clone(),samples]}];
    let mut piano=encode::build(&Donor::Rules(Rules::default()),&Options::new("Decay regression"),&recordings).unwrap().to_piano().unwrap();
    // Explicitly cover the octave used by the transposition regression.
    piano.file.body.0[0x8c+72]=60;
    let reference={let lib=piano.library().unwrap();*lib.strokes()[0].record()};
    let offsets=piano.file.body.0.windows(118).enumerate().filter_map(|(i,r)|(r==reference).then_some(i)).collect::<Vec<_>>();
    assert_eq!(offsets.len(),1);let offset=offsets[0];
    let record=&mut piano.file.body.0[offset..offset+118];
    for (at,value) in [(0x1c,35_000u32),(0x20,35_000),(0x24,34_900),(0x28,35_100)]{record[at..at+4].copy_from_slice(&value.to_be_bytes());}
    // 0x3a controls the pre-loop phase; 0x2e controls the loop phase.
    for (at,db) in [(0x3a,1.0f64),(0x2e,6.0)]{
        let value=(8_388_608.0*10f64.powf(-db/(20.0*21680.0))).round()as u32;
        record[at..at+4].copy_from_slice(&value.to_be_bytes());
    }
    record[0x34..0x36].copy_from_slice(&0i16.to_be_bytes());
    piano.write_to(&mut std::fs::File::create(destination).unwrap()).unwrap();
    println!("{} Hz fixture, 1 dB/s before loop, 6 dB/s in loop",codec::RATE);
}
