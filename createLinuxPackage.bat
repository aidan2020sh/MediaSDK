@echo off
IF "%~1%"=="" (
	echo Target Dir is not set
	echo usage: createLinuxPackage.bat ^<TargetDir^> ^<BinarySourceDir^> ^<DocSourceDir^>
	exit /b
)

IF "%~2%"=="" (
	echo Binary Source Dir is not set
	echo usage: createLinuxPackage.bat ^<TargetDir^> ^<BinarySourceDir^> ^<DocSourceDir^>
	exit /b
)

IF "%~3%"=="" (
	echo Binary Source Dir is not set
	echo usage: createLinuxPackage.bat ^<TargetDir^> ^<BinarySourceDir^> ^<DocSourceDir^>
	exit /b
)

xcopy /S builder\*.* %1\builder\

xcopy /S samples\*.c %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S samples\*.cl %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S samples\*.cmake %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S samples\*.cpp %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S samples\*.cproject %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S samples\*.def %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S samples\*.h %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S samples\*.hpp %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S samples\*.map %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S samples\*.pdf %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S samples\*.pl %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S samples\*.project %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S samples\*.so %1\samples\ /EXCLUDE:exclusions.txt
xcopy /S samples\*.txt %1\samples\ /EXCLUDE:exclusions.txt


xcopy %2\sample_decode_drm %1\samples\_bin\x64\*
xcopy %2\sample_decode_x11 %1\samples\_bin\x64\*
xcopy %2\sample_decvpp_drm %1\samples\_bin\x64\*
xcopy %2\sample_decvpp_x11 %1\samples\_bin\x64\*
xcopy %2\sample_encode_drm %1\samples\_bin\x64\*
xcopy %2\sample_encode_x11 %1\samples\_bin\x64\*
xcopy %2\sample_h265_gaa_drm %1\samples\_bin\x64\*
xcopy %2\sample_h265_gaa_x11 %1\samples\_bin\x64\*
xcopy %2\sample_multi_transcode_drm %1\samples\_bin\x64\*
xcopy %2\sample_multi_transcode_x11 %1\samples\_bin\x64\*
xcopy %2\sample_vpp_drm %1\samples\_bin\x64\*
xcopy %2\sample_vpp_x11 %1\samples\_bin\x64\*
xcopy %2\ocl_rotate.cl %1\samples\_bin\x64\*
xcopy %2\libsample_plugin_opencl.so %1\samples\_bin\x64\*
xcopy %2\libsample_rotate_plugin.so %1\samples\_bin\x64\*

xcopy /S %3\*.pdf %1\samples\
move "%1\samples\Media Samples Guide.pdf" "%1\Media_Samples_Guide.pdf"