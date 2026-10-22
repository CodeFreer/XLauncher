# XLauncher
利用Duck.ai指导，编写的一个小程序，用于创建虚拟盘并启动程序

**作用：**
以如下格式的 `config.INI` 作为配置文件

```ini
[RUN]
Vdisk=X:
RunCmd=Folder1\Folder12\File123.exe
```

项目假设结构如下：

D:\X\  假设生成的启动器放在这个目录中
 +- launcher.exe  本项目生成的启动器
 +- config.ini  启动器使用的配置文件
 +- Folder1\  这是用于放置要被启动的程序的文件夹，文件夹的名称任意，这就是配置文件中 RunCmd 中指定的运行命令的路径
   +- Folder12\
     +- File123.exe ...
         
启动 Launcher.exe 后，会把程序所在目录(示例中为 `D:\X`)虚拟成 **Vdisk** 指定盘符 X: 的虚拟盘，然后根据 **RunCmd** 运行 `X:\Folder1\Folder12\File123.exe` (实际对应于 `D:\X\Folder1\Folder12\File123.exe`)
