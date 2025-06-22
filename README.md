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
        -auto-sync: Automatically find and synchronize shareable directories across volumes.
```

## Automatic Synchronization Across Volumes

FileSync can automatically detect and synchronize related directories across all connected storage volumes (e.g., internal hard drives, SSDs, USB drives).

**How it Works:**

1.  **Volume Detection**: The tool scans for all accessible fixed and removable storage volumes.
2.  **Shareable Directory Identification**: On each volume, it looks for directories that contain a special marker file named `nodesFile.fs`. This file is automatically created and managed by FileSync when you use operations like `-sync`, `-copy`, `-scan`, or `-check` on a directory. A directory containing `nodesFile.fs` is considered a "shareable root".
3.  **Grouping**: Shareable root directories located on different volumes that share the **same base directory name** are considered "related" and are grouped together. For example, if you have `/media/usb1/MyProject` and `/home/user/BackupSSD/MyProject`, both containing `nodesFile.fs`, they will be grouped because their base name is "MyProject".
4.  **Synchronization**: Within each group of related directories, FileSync performs pairwise synchronizations. For a group with directories D1, D2, and D3, it will effectively synchronize D1 with D2, D1 with D3, and D2 with D3, ensuring all directories in the group become consistent with each other based on the latest changes.

**Usage:**

To trigger the automatic synchronization feature, use the `--auto-sync` flag:

    filesync --auto-sync

**Behavioral Flags with Auto-Sync:**

*   `--nocheck`: When used with `--auto-sync`, this tells the underlying synchronization operations not to rescan the directories for changes before comparing them. It relies on the existing information in `nodesFile.fs`. This can be faster but might miss very recent changes not yet recorded. (Default is to re-scan).
*   `--dummy`: When used with `--auto-sync`, this performs a "dry run." The tool will identify volumes, shareable directories, and groups, and it will print the synchronization actions it *would* take, but no actual file operations will be performed. This is useful for verifying what will happen before committing to changes.

**Preparing Directories for Auto-Sync:**

For a directory to be picked up by `--auto-sync`, it must have been previously managed by FileSync (e.g., by running `filesync -check -dir /path/to/yourdir` or using it in a `-sync` or `-copy` operation). This creates the necessary `nodesFile.fs` marker file. Ensure that directories you want to auto-synchronize have the same base name on different volumes.

**Example Scenario:**

1.  You have a project directory: `/home/user/Work/MyProject`.
    *   Run `filesync -check -dir /home/user/Work/MyProject` once to initialize it.
2.  You have a USB drive mounted at `/media/myusb`. You copy `MyProject` to it, so you have `/media/myusb/MyProject`.
    *   Run `filesync -check -dir /media/myusb/MyProject` once to initialize it on the USB.
3.  Now, with both locations initialized and containing `nodesFile.fs`:
    *   Running `filesync --auto-sync` will detect these two "MyProject" directories and synchronize their contents.
    *   If you later plug in another backup drive and create `/mnt/backup_drive/MyProject` (and initialize it), `--auto-sync` will then synchronize all three.

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