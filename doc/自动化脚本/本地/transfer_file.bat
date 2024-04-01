
@echo off
setlocal

::今天的日期YYYYMMDD
for /f "tokens=1-3 delims=/" %%a in ("%date%") do (
    set "year=%%a"
    set "month=%%b"
    set "day=%%c"
)
set "formatted_date=%year%%month%%day:~0,2%"
::echo %formatted_date%

::这里需要根据实际情况修改
set WINSCP_PATH=C:\Users\admin\AppData\Local\Programs\WinSCP\WinSCP.com
::本地路径，这里需要根据实际情况修改，路径最后带 \
set LOCAL_PATH=D:\Document\code\script_receiveFile\data\

::在本地目录下创建日期命名的文件夹
set LOCAL_PATH_DATE=%LOCAL_PATH%%formatted_date%\
::echo %LOCAL_PATH_DATE%
mkdir %LOCAL_PATH_DATE%

::生产环境信息
set USERNAME=huangjinpeng
set PASSWORD=123456
set HOST=114.141.173.67
set PORT=30049
set REMOTE_FILE=/mnt/dbfile/huangjinpeng/binData/mkdts/%formatted_date%/mkdt*
::set REMOTE_FILE=/mnt/dbfile/huangjinpeng/binData/mkdts/mkdt*
::set REMOTE_FILE=/mnt/dbfile/huangjinpeng/test/mkdts/mkdt*

::下载
echo open sftp://%USERNAME%:%PASSWORD%@%HOST%:%PORT%/ > sftp_temp.bat
echo option batch on >> sftp_temp.bat
echo option confirm off >> sftp_temp.bat
echo option transfer binary >> sftp_temp.bat
::下载分片文件，下载完的删除，如果下载中断，重新执行脚本不会重复下载
echo get -delete %REMOTE_FILE% %LOCAL_PATH_DATE% >> sftp_temp.bat
::echo get %REMOTE_FILE% %LOCAL_PATH_DATE% >> sftp_temp.bat
echo exit >> sftp_temp.bat

"%WINSCP_PATH%" /log=downLoad.log /script=sftp_temp.bat
del sftp_temp.bat

::56服务器登录信息
set UPLOAD_USERNAME=huangjp
set UPLOAD_PASSWORD=Fsl231016
set UPLOAD_HOST=192.168.173.56
set UPLOAD_PORT=22
set UPLOAD_REMOTE_PATH=/mnt/disk4/StrategyPlatformData/shengchan/mkdts/
::set UPLOAD_REMOTE_PATH=/mnt/disk4/StrategyPlatformData/test/mkdts/
set UPLOAD_LOCAL_FILE=%LOCAL_PATH_DATE%mkdt*

::建立连接
echo open sftp://%UPLOAD_USERNAME%:%UPLOAD_PASSWORD%@%UPLOAD_HOST%:%UPLOAD_PORT%/ > sftp02_temp.bat
echo option batch on >> sftp02_temp.bat
echo option confirm off >> sftp02_temp.bat
echo option transfer binary >> sftp02_temp.bat

::创建新目录
echo mkdir %UPLOAD_REMOTE_PATH%%formatted_date% >> sftp02_temp.bat

::上传分片文件，上传完的删除
echo put -delete %UPLOAD_LOCAL_FILE% %UPLOAD_REMOTE_PATH%%formatted_date%/ >> sftp02_temp.bat
::echo put %UPLOAD_LOCAL_FILE% %UPLOAD_REMOTE_PATH%%formatted_date%/ >> sftp02_temp.bat
echo exit >> sftp02_temp.bat

"%WINSCP_PATH%" /log=upLoad.log /script=sftp02_temp.bat
del sftp02_temp.bat

endlocal