## Head
This is used for our compilor project, and welcome to talk.

## Build

The Lab1's file is in `./lab1/src`
The Lab2's file is in `./lab2/src`
The Lab3's file is in `./lab3/src`

```bash
$ cd lab1,2,3
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

test compiler with `test.py`:

lab1:

```bash
# step 1. build
cd lab1
mkdir -p build && cd build
cmake ..
make
# step 2. test
cd ../../tests
python3 test.py ../lab1/build/compiler lab1 --executor_path test.py
cd ..
```

lab2:

```bash
cd lab2
mkdir -p build && cd build
cmake ..
make
cd ../../tests
python3 test.py ../lab2/build/compiler lab2 --executor_path test.py
cd ..
```

lab3:

Note. a rust environment is needed for lab3 tester, and the `accipit` should be compiled before testing. If any issue occurs while compiling `accipit`, you can try to run our compiled `accipit` in `accipit/` dir.

NNote. the `Cargo.lock` file is especially important for compiling rust code in the future, so please do not remove it from git repository. (commented by 2026.03.18)

```bash
cargo build --locked
cd lab3
mkdir -p build && cd build
cmake ..
make
cd ../../tests
python3 test.py ../lab3/build/compiler lab3
cd ..
```

If you use the compiled `accipit` in `accipit/` dir, you can set the `executor_path` to `./accipit/accipit-darwin-arm64`:

```bash
python3 test.py ../lab3/build/compiler lab3 --executor_path ../accipit/accipit-darwin-arm64
```