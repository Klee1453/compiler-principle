## Head
This is used for our compilor project, and welcome to talk.

## Build

The Lab1's file is in `./lab1/src`
The Lab2's file is in `./lab2/src`
The Lab3's file is in `./lab3/src`

```bash
$ cd lab\*
$ rm -rf build
$ mkdir build
$ cd build
$ cmake ..
$ make
$ ./compilor
```

## lab3
This part we translate the AST to IR code, use the tool of IR, in `./lab3/accsys/`.

There is a tool to execute the IR, like `test.acc`, in the dir `target/debug/accipit`

```bash
$ ./target/debug/accipit ./tests/IR/test.acc
```

## Notes

clean all before build or commit:

```bash
rm -rf lab1/build lab2/build lab3/build
```

test your compiler:

```bash
# test.py usage for lab1
# =========================
# step 1. build your compiler
cd lab1
mkdir -p build && cd build
cmake ..
make
# step 2. test your compiler
cd ../../tests
python3 test.py ../lab1/build/compiler lab1 --executor_path test.py
cd ..

# test.py usage for lab2
# =========================
cd lab2
mkdir -p build && cd build
cmake ..
make
cd ../../tests
python3 test.py ../lab2/build/compiler lab2 --executor_path test.py
cd ..

# a rust environment is needed for lab3 tester, 
# and the `accipit` should be compiled before testing.
# test.py usage for lab3
# =========================
cargo build --manifest-path Cargo.toml
cd lab3
mkdir -p build && cd build
cmake ..
make
cd ../../tests
python3 test.py ../lab3/build/compiler lab3
cd ..
```

If any issue happens while compiling `accipit`, you can try my compiled version, architecture: `Mach-O 64-bit executable arm64`, path: `./lab3/accipit`. The test command should be modified to `python3 test.py ../lab3/build/compiler lab3 --executor_path ../lab3/accipit`.