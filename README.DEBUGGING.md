### Requirements: ###

* llvm-dsymutil (>= 3.8)
* A macOS system with lldb / gdb installed

### Setting up llvm-dsymutil: ###

Install `llvm-dsymutil` (>= 3.8) from your host system's LLVM packages and ensure
that `llvm-dsymutil` or `dsymutil` is available in `PATH` BEFORE building OSXCross.

### Debug Example: ###

* Build your application with `-g`
* [LTO only] Add `-Wl,-object_path_lto,lto.o` to the linker flags
* After linking run: `dsymutil binary`
* [Optional] Strip the binary: `x86_64-apple-darwinXX-strip binary`
* Copy the binary **and** the created `<binary>.dSYM` "folder" onto the target macOS system
* Debug the binary as usual
