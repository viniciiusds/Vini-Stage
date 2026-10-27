//! Create a small self-generated piano fixture for independent-layer regression tests.
use nord_format::formats::npno::{codec,encode::{self,Donor,Options,Recording,Rules},Bank};
fn main(){
    let destination=std::env::args().nth(1).expect("Output .npno path");
    let count=(codec::RATE as f64*0.8)as usize;
    let samples=(0..count).map(|i|{
        let t=i as f64/codec::RATE as f64;
        (6000.0*(std::f64::consts::TAU*261.625565*t).sin()*(-2.0*t).exp())as i16
    }).collect::<Vec<_>>();
    let recordings=[Recording{root:60,bank:Bank::Attack,layer:0,channels:vec![samples.clone(),samples]}];
    let library=encode::build(&Donor::Rules(Rules::default()),&Options::new("Test sine"),&recordings).unwrap();
    library.to_piano().unwrap().write_to(&mut std::fs::File::create(destination).unwrap()).unwrap();
}
