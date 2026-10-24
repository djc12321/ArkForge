# OpenHarmony resource compile tool  

## Description
resource tool is used in computer.In OpenHarmony SDK toolchain.Provide to IDE.Support Windows, Linux, MacOS platform.

## Directory Structure

```
/developtools
|----global_resource_tool
|    |----include         #Header file
|    |----src             #source code file
|    |----test            #Test case
|    |----build           #Depending on the third-party library compilation script
|    |----BUILD.gn        #Compile script
|    |----CMakeLists.txt  #CMake file
|    |----win32.cmake     #Windows cross compilation script
```

## Instructions

### Test

In PC, run `python test.py ./restool ./out`  

### Help

resource tool usually integrate to IDE, OpenHarmony compile system.

resource tool command e.g:  

-v version  
-i input resource path  
-o output resource path  
-r resource ID header file path  
-p package name  
-f if output path exists,force override it  
-m module name, can add more, split by ','(like entry1,entry2,...)  
-j config.json path  
-e start id mask, e.g 0x01000000, in [0x01000000, 0x06FFFFFF),[0x08000000, 0x41FFFFFF)  

```
entry Directory Structure
|    |----resource
|    |----config.json or module.json
```
`./restool -i entry1 -i entry2 -j xxx/module.json -p ohos.demo.xxx -o out -r out/ResourceTable.h `

##  Related to the storehouse

**global_resource_tool**

[third_party_cJSON](https://gitee.com/openharmony/third_party_cJSON/blob/master/README.md)

[third_party_libpng](https://gitee.com/openharmony/third_party_libpng/blob/master/README.md)

[third_party_bounds_checking_function](https://gitee.com/openharmony/third_party_bounds_checking_function/blob/master/README.en.md)

