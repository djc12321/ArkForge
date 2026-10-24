# OpenHarmony资源编译工具

## 简介
restool（资源编译工具）是一种资源构建工具。通过编译资源文件创建资源索引、解析资源。restool保存在sdk安装目录下的toolchains子目录。

## 代码目录

```
/developtools
|----global_resource_tool
|    |----include         #头文件
|    |----src             #源代码文件
|    |----test            #测试用例
|    |----build           #依赖三方库编译脚本  
|    |----BUILD.gn        #编译脚本
|    |----CMakeLists.txt  #CMake文件
|    |----win32.cmake     #Windows交叉编译脚本  
```

## 使用说明

### SDK编译命令

[SDK编译命令参考](https://gitcode.com/openharmony/build/blob/master/README_zh.md)

### 参数说明

#### restool支持的命令选项

| 选项 | 是否可缺省 | 是否存在入参 | 描述 |
| -------- | -------- | -------- | -------- |
| -i/--inputPath | 不可缺省 | 带参数 | 指定需要构建的资源目录或者资源中间件。<br>在资源目录中支持指定一个编译好的HAP/HSP资源目录（解压态），在此HAP/HSP的基础上完成叠加编译。<br>具体可参考如下[编译资源命令](#编译资源)。 |
| -j/--json | 不可缺省 | 带参数 | 指定config.json或者module.json文件路径。 |
| -o/--outputPath | 不可缺省 | 带参数 | 指定已编译资源的输出路径。 |
| -p/--packageName | 不可缺省 | 带参数 | 指定编译资源的bundle名称。 |
| -r/--resHeader | 不可缺省 | 带参数 | 指定资源的头文件生成路径，文件内容为资源名称与资源ID的映射表，支持4种格式：“.txt”、“.js”、“.h”、“.ts”。<br>**说明：**<br>- “.txt”、“.js”、“.h”格式的文件包含-i指定的所有资源目录下的资源映射表。<br>- 从API version 23开始，支持“.ts”格式，文件内容仅包含-i指定的HAR编译产物的资源目录下的资源映射表。|
| -e/--startId | 可缺省 | 带参数 | 指定生成资源的起始ID值，例如：0x01000000,范围[0x01000000, 0x06FFFFFF),[0x08000000, 0xFFFFFFFF)。 |
| -f/--forceWrite | 可缺省 | 不带参数 | 如果输出路径已经存在。强制删除，重新生成。 |
| -h/--help | 可缺省 | 不带参数 | 查看工具帮助信息。 |
| -m/--modules | 可缺省 | 带参数 | 多模块联合编译时，指定多个模块名。用“,”连接。 |
| -x/--append | 可缺省 | 带参数 | 指定生成中间文件的资源目录或单个资源路径。同一个命令可以多次指定。 |
| -z/--combine | 可缺省 | 不带参数 | 针对资源中间文件目录，生成编译结果。 |
| -l/--fileList | 可缺省 | 带参数 | 指定命令选项的集合json文件，例如：resConfig.json。详细格式请查看下方-l/--fileList参数说明。 |
| -v/--version | 可缺省 | 不带参数 | 查看工具版本号。 |
| --ids | 可缺省 | 带参数 | 指定生成id_defined.json的输出目录。 |
| --defined-ids | 可缺省 | 带参数 | 指定id_defined.json文件路径，一般都是通过--ids生成。<br>id_defined.json包含资源类型、名称及其ID的列表。<br>开发者可以自定义id_defined.json中的资源ID。 |
| --dependEntry | 可缺省 | 带参数 | FA模型下单独编译feature时，指定entry模块的编译结果目录。 |
| --icon-check | 可缺省 | 不带参数 | 开启icon和startWindowIcon的PNG图片校验功能。 |
| --compressed-config | 可缺省 | 带参数 | 指定需要进行纹理压缩的json配置文件。例如：opt-compression.json，json配置文件的详细格式请查看下方--compressed-config参数说明。|
| --thread | 可缺省 | 带参数 | 指定资源编译时开启的子线程数量。 <br>**说明：**<br> 从API version 18开始，支持该选项。|
| --target-config | 可缺省 | 带参数 | 与“-i”命令同时使用，支持选择编译。<br>具体可参考如下**target-config参数说明**。|
| --ignored-file | 可缺省 | 带参数 | 指定资源文件和资源目录的忽略规则，格式为正则表达式，多个规则之间以“:”分隔。文件、目录的名称与正则表达式匹配的会被忽略。<br>例如：“\\.git:\\.svn”可以忽略所有名称为“.git”、“.svn”的文件和目录。<br>**说明：**<br> 从API version 19开始，支持该选项。|
| --ignored-path | 可缺省 | 带参数 | 指定资源文件和资源目录的忽略规则，格式为正则表达式，多个规则之间以“:”分隔。文件、目录的名称或路径与正则表达式匹配的会被忽略。<br>例如：“.+/rawfile/\\.git:\\.svn”中第一个正则包含指定路径“.+/rawfile/”，可以忽略rawfile目录下的“.git”文件和目录，不会忽略其他目录下的“.git”文件和目录；第二个规则不包含任何指定路径，可以忽略所有名称为“.svn”的文件和目录。<br>**说明：**<br> 从API version 23开始，支持该选项。|


**target-config参数说明**

支持参数配置类型：MccMnc、Locale、Orientation、Device、ColorMode、Density

参数格式说明：配置之间用“;”分割，配置中的值用“[]”封装，并使用“,”分割。

MccMnc匹配规则：Mcc（国家码）必须相同；Mnc（网络码）不存在时默认匹配，否则Mnc须相同才匹配。

Locale匹配规则：Locale匹配需满足以下三条规则。

1、语言须相同。

2、脚本（文字）不存在时默认匹配，否则必须相同。

3、国家或地区不存在时默认匹配，否则必须相同。

参数举例说明：Locale[zh_CN,en_US];Device[phone]，该参数过滤其他语言，保留能匹配上zh_CN和en_US的语言；过滤其他设备，保留phone；其他参数（如MccMnc、Orientation等）配置不过滤均保留。

**-l/--fileList参数说明**

用于指定命令选项集合的json文件，json文件包含的字段与命令选项的对应关系如下。

| 字段名称 | 类型 | 命令选项 | 描述 |
| -- | -- | -- | -- |
| configPath | string | -j/--json | 请参考-j/--json的说明。 |
| packageName | string | -p/--packageName | 请参考-p/--packageName的说明。 |
| output | string | -o/--outputPath | 请参考-o/--outputPath的说明。 |
| startId | string | -e/--startId | 请参考-e/--startId的说明。 |
| moduleNames | string | -m/--modules | 请参考-o/--outputPath的说明。 |
| ResourceTable | string[] | -r/--resHeader | 可以包含多个路径，相当于多次指定-r/--resHeader。 |
| applicationResource | string | -i/--inputPath | 指定AppScope的资源目录。 |
| moduleResources | string[] | -i/--inputPath | 指定当前模块的资源目录，可以包含多个目录，相当于多次指定-i/--inputPath。 |
| dependencies | string[] | -i/--inputPath | 指定依赖模块的资源目录，可以包含多个目录，相当于多次指定-i/--inputPath。 |
| entryCompiledResource | string | --dependEntry | 请参考--dependEntry的说明。 |
| iconCheck | boolean | --icon-check | 是否启用icon和startWindowIcon的PNG图片校验功能。<br> - true：启用。<br>- false（缺省默认值）：不启用。|
| ids | string | --ids | 请参考--ids的说明。 |
| definedIds | string | --defined-ids | 请参考--defined-ids的说明。 |
| compression | string | --compressed-config | 请参考--compressed-config的说明。 |
| thread | integer | --thread | 请参考--thread的说明。 |
| ignoreResourcePattern | string[] | --ignored-file | 请参考--ignored-file的说明。 |
| ignoreResourcePathPattern | string[] | --ignored-path | 请参考--ignored-path的说明。 |
| qualifiersConfig | object | --target-config | 指定选择编译的参数配置，格式为json，支持的字段与`--target-config`的配置类型一致，字段类型为字符串数组，表示一个配置类型下可以配置多个值。举例说明：`{"Locale":["zh_CN","en_US"], "Device":["phone"]}`等同于`--target-config`的配置`Locale[zh_CN,en_US];Device[phone]`。 <br>**说明：**<br> 从API version 23开始，支持该字段。|

**--compressed-config参数说明**

在DevEco Studio配置[纹理压缩](https://developer.huawei.com/consumer/cn/doc/harmonyos-guides/ide-hvigor-build-profile#section2095319147103)的编译配置参数，编译后会在模块的`build\default\intermediates\res\default`下生成restool的纹理压缩配置文件opt-compression.json，文件结构如下：

```json5
{
  "context": {
    // 纹理压缩库的绝对路径
    "extensionPath": "xxx\\xxx\\libimage_transcoder_shared.dll"
  },
  "compression": {
    // 是否对预置media图片资源启用纹理压缩。true：启用，false：不启用。
    "media": {
      "enable": true
    },
    // filters是预置media图片资源的过滤配置参数
    "filters": [
      {
        // 纹理压缩的方式，type为转换类型，支持"astc"、"sut"
        // block为转换扩展参数，决定画质和压缩率，当前仅支持"4x4"
        "method": {
          "blocks": "4x4",
          "type": "astc"
        },
        // 需要压缩的media图片资源文件绝对路径
        "path": [
          "xxx\\MyApplication\\entry\\src\\main\\resources\\base\\media\\startIcon.png",
          "xxx\\MyApplication\\entry\\src\\main\\resources\\base\\media\\icon.png",
        ],
        // 需要过滤的media图片资源文件绝对路径
        "exclude_path": [],
        // 按大小和分辨率匹配path中的图片资源文件路径，符合条件的文件将被压缩
        "rules_origin": {
          // 二维数组，数组中每个元素表示一个大小范围，单位为字节
          "size": [
            [
              0,
              10485760
            ]
          ],
          // 二维数组，数组中每个元素表示一个分辨率范围，如下表示分辨率从0x0到1024x1024
          "resolution": [
            [
              {
                "height": 0,
                "width": 0
              },
              {
                "height": 1024,
                "width": 1024
              }
            ]
          ]
        },
        // 按大小和分辨率匹配exclude_path中的图片资源文件，符合条件的文件将被过滤
        "rules_exclude": {
          "size": [
            [
              0,
              1048576
            ]
          ],
          "resolution": [
            [
              {
                "height": 0,
                "width": 0
              },
              {
                "height": 64,
                "width": 64
              }
            ]
          ]
        }
      }
    ]
  }
}

```

#### restool支持的子命令

| 命令  | 描述 |
| ---  | ---- |
| dump | 以json的格式输出HAP包中resource的内容。 |

**dump命令**

```
restool dump [-h] [config] filePath
```
dump命令参数列表

| 参数 | 是否可缺省 | 是否存在入参 | 描述 |
| --- | --------- | ---------- |------- |
| -h  | 可缺省 | 不带参数 | 帮助信息。 |
| config | 可缺省 | 不带参数 | 只打印HAP包中资源的限定词信息。 |

示例:

```sh
# 打印HAP包中所有的资源信息
restool dump entry.hap
# 打印HAP包中资源的限定词信息
restool dump config entry.hap
```

### 使用实例

例如：entry目录结构如下
```
entry/src/main
|    |----resource
|    |    |----base
|    |    |    |----element
|    |    |    |----media
|    |    |    |----profile
|    |    |----rawfile
|    |----config.json/module.json
```

构建资源全量命令：

```
restool -i entry/src/main  -j entry/src/main/module.json -p com.ohos.demo -o out -r out/ResourceTable.txt -f
```

构建资源增量命令(仅预览模式可用)，具体步骤如下：

1.生成资源中间件:
```
restool -x entry/src/main/resource -o out
```
2.编译中间件:
```
restool -i out1 -i out2 -o out -p com.ohos.demo -r out/ResourceTable.txt -j entry/src/main/module.json -f -z
```

叠加资源编译命令：

```
# hapResource为解压后的HAP包路径
restool -i entry/src/main -i hapResource -j entry/src/main/module.json -p com.ohos.demo -o out -r out/ResourceTable.txt -f
```

固定资源ID的方式有两种，如下：

方式一：在resource/base/element/目录下存放自定义id_defined.json文件。构建成功后，生成的ID值将会和id_defined.json文件中自定义的ID值保持一致。

方式二：通过--ids 命令生成id_defined.json文件。--defined-ids命令指定id_defined.json文件。构建成功后，生成的ID值将会和id_defined.json文件中自定义的ID值保持一致。

生成id_defined.json文件：
```
restool -i entry/src/main  -j entry/src/main/module.json -p com.ohos.demo -o out -r out/ResourceTable.txt --ids out -f
```

指定资源ID固定的id_defined.json文件：
```
restool -i entry/src/main  -j entry/src/main/module.json -p com.ohos.demo -o out1 -r out1/ResourceTable.txt --defined-ids out/id_defined.json -f
```

### 资源类型介绍
[资源类型介绍](https://gitcode.com/openharmony/docs/blob/master/zh-cn/application-dev/quick-start/resource-categories-and-access.md)

## 相关仓

**global_resource_tool**

[third_party_cJSON](https://gitcode.com/openharmony/third_party_cJSON/blob/master/README.md)

[third_party_libpng](https://gitcode.com/openharmony/third_party_libpng/blob/master/README)

[third_party_bounds_checking_function](https://gitcode.com/openharmony/third_party_bounds_checking_function/blob/master/README.md )
