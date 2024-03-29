# FileSync
Simple local filesystem file synchronization. For use with external devices (usbdisk) and SneakerNet like usage.

## Installation
Copy the resulting executable to the desired location.

The executable is completelly portable, there are no dependencies.

## Usage
Basic usage to syncronize two directories, dirA and dirB:

    filesync -sync -dir dirA -dir dirB

To make a efficient copy from dirA to dirB:

    filesync -copy -dir dirA -dir dirB

The rest of options are listed with no parameters:

```
$ filesync
Parameters:
        -dir [Item]: Specify a directory.
        -nocheck: Do not check for changes on directories.
        -dummy: Do not perform operations.
        -copy: Copy first directory to second directory.
        -sync: Synchronize between two directories.
        -log [Item]: Log actions to file.
        -scan [Item] [Item]: Scan directory and save to filenode file.
        -rescan [Item] [Item]: Rescan directory and save to filenode file.
        -read [Item]: Read filenode file.
        -check [Item]: Check changes on a directory.
```

## Building
There is a GNU Make compatible Makefile usable on Linux and MingGW.

    make

## Contributing
1. Fork it!
2. Create your feature branch: `git checkout -b my-new-feature`
3. Commit your changes: `git commit -am 'Add some feature'`
4. Push to the branch: `git push origin my-new-feature`
5. Submit a pull request :D

## Credits
* Valeriano Alfonso Rodriguez.

## License

    The MIT License (MIT)

    Copyright (c) 2014-2021 Valeriano Alfonso Rodriguez

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.