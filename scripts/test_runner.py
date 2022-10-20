from subprocess import Popen
from time import sleep

qemu = Popen(["qemu-system-i386", "-m", "64M", "-display", "none",
              "-chardev", f"file,id=logfile,path=test.txt",
              "-serial", "chardev:logfile", "-cdrom", "os.iso"])

sleep(1)
qemu.kill()

with open("test.txt", "rb") as f:
    f.seek(-len(b"TEST_SUCESS") - 1, 2)
    if f.read(len(b"TEST_SUCCESS")) == b"TEST_SUCCESS":
        print("Test successful")
    else:
        print("Test failed")
        f.seek(0, 0)
        print(f.read().decode("utf-8"))
