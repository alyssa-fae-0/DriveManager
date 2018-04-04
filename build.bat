cls
if not exist build (mkdir build)

pushd build


set compiler_flags=/EHsc /Zc:wchar_t /nologo
set includes=/I"../code"  /I"C:\api\wxwidgets_src\include"
set defines=/D"__WXMSW__" /D"UNICODE" /D"_UNICODE" /D"_DEBUG"
@rem /D"WXUSINGDLL"
set linker_options=/LIBPATH:"C:\api\wx\lib\vc_x64_lib"
set libraries=/DYNAMICBASE "wxmsw31ud.lib wxtiff.lib wxjpegd.lib wxpngd.lib wxzlibd.lib wxregexud.lib wxexpatd.lib"
cl %defines% %compiler_flags% %includes% ../code/main.cpp /link %linker_options%


popd

@rem /I"C:\api\wxwidgets_src\include\msvc"

@rem compiler: /permissive- /GS /analyze- /W3 /Zc:wchar_t /I"C:\api\wx\include\msvc" /I"C:\api\wx\include" /I"C:\api\catch" /Zi /Gm- /Od /sdl /Fd"Debug\vc141.pdb" /fp:precise /D "WXUSINGDLL" /D "wxMSVC_VERSION_AUTO" /D "_UNICODE" /D "UNICODE" /D "_MBCS" /errorReport:prompt /WX /Zc:forScope /RTC1 /Gd /Oy- /MDd /FC /Fa"Debug\" /EHsc /nologo /Fo"Debug\" /Fp"Debug\DriveManager.pch" /diagnostics:classic 
@rem linker: /OUT:"C:\dev\projects\DriveManager\Debug\DriveManager.exe" /MANIFEST /NXCOMPAT /PDB:"C:\dev\projects\DriveManager\Debug\DriveManager.pdb" /DYNAMICBASE "kernel32.lib" "user32.lib" "gdi32.lib" "winspool.lib" "comdlg32.lib" "advapi32.lib" "shell32.lib" "ole32.lib" "oleaut32.lib" "uuid.lib" "odbc32.lib" "odbccp32.lib" /DEBUG:FASTLINK /MACHINE:X86 /INCREMENTAL /PGD:"C:\dev\projects\DriveManager\Debug\DriveManager.pgd" /MANIFESTUAC:"level='asInvoker' uiAccess='false'" /ManifestFile:"Debug\DriveManager.exe.intermediate.manifest" /ERRORREPORT:PROMPT /NOLOGO /LIBPATH:"C:\api\wx\lib\vc141_dll" /TLBID:1 