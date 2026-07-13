# crash

## Support only linux

## Support CHIP

|      CHIP        | Remark |
|:----------------:|:------:|
|qcc743/qcc744       |        |


## Compile

- qcc743/qcc744

```
make CHIP=qcc743 BOARD=qcc743dk
```

## tools

|      OS          | tools suffix|
|:----------------:|:-----------:|
|ubuntu            |-ubuntu      |


## perl environment

in linux, perl is POSIX standard environment.


## Run environment

|      OS          | env               |
|:----------------:|:-----------------:|
|ubuntu            |bash terminal      |


## Flash

```
make flash CHIP=chip_name COMX=xxx # xxx is your com name
```

## coredump in flash usage

1. Enter crash in cli, wait log print. At the same time, a coredump will be generated in the core partition.

2. Get bin format crash binary  (press CHIP_EN, click RESET and release CHIP_EN)

```
(qcc743dk)
./tools/qcc74x_tools/QConn_Flash/QConn_Flash_Cmd-ubuntu --port your-serial-port --read --start your-fw-part-last-16K --len your-partition-size --file crash.bin
mv tools/qcc74x_tools/QConn_Flash/crash.bin .
```

3. Start Debug

```
./tools/byai/coredump.py crash.bin your.elf
```

## coredump in log usage

Only one line, './tools/byai/coredump.py your_log.txt your.elf'


## Note

1. please kill bugkiller_linux_amd64 process after debug.

2. Because of the driver of linux ch347, after a crash outputs a large amount of logs, the devices needs to be unplugged to make ch347 work correctly.
