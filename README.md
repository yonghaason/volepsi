### Build

Tested on Ubuntu 20.04.6 LTS.

The library can be cloned and built with networking support as
```
git clone https://github.com/Visa-Research/volepsi.git
cd volepsi
python3 build.py -DVOLE_PSI_ENABLE_BOOST=ON
```

### Test

After build, you can cd to test executable
```
cd out/build/linux/frontend
```
Then
```
./frontend -h
```
will show help message, and 
```
./frontend -u -list
```
will show test lists.

For example, you may run PSU test with
```
./frontend -u 5 -nn 20
```


