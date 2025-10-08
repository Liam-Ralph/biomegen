// Main Function

fn main() {

    // Setting Process Title

    proctitle::set_title("BiomeGen Autorun");

    println!();

    // Getting Program Version

    let contents = std::fs::read_to_string("README.md").expect("");

}