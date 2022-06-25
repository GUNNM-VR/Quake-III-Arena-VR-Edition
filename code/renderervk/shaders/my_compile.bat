rem echo off

set glslc_exe=D:/android/android-sdk/ndk/21.0.6113669/shader-tools/windows-x86_64/glslc.exe

mkdir %~dp0spirv
del /Q %~dp0spirv\shader_data.c
del /Q %tmpf%

set bh=%~dp0bin2hex.exe
set outf=+spirv\shader_data.c
set tmpf=%~dp0spirv\data.spv

rem compile individual shaders

 for %%f in (*.vert) do (
 	%glslc_exe% %%f -o %tmpf%
 	%bh% %tmpf% %outf% %%~nf_vert_spv
 	del /Q %tmpf%
 )

 for %%f in (*.frag) do (
 	%glslc_exe% %%f -o %tmpf%
 	%bh% %tmpf% %outf% %%~nf_frag_spv
 	del /Q %tmpf%
 )

rem compile lighting shader variations from templates

%glslc_exe% -fshader-stage=vertex light_vert.tmpl -o %tmpf%
%bh% %tmpf% %outf% vert_light

%glslc_exe% -fshader-stage=vertex -DUSE_FOG light_vert.tmpl -o %tmpf%
%bh% %tmpf% %outf% vert_light_fog

%glslc_exe% -fshader-stage=fragment light_frag.tmpl -o %tmpf%
%bh% %tmpf% %outf% frag_light

%glslc_exe% -fshader-stage=fragment -DUSE_FOG light_frag.tmpl -o %tmpf%
%bh% %tmpf% %outf% frag_light_fog

%glslc_exe% -fshader-stage=fragment -DUSE_LINE light_frag.tmpl -o %tmpf%
%bh% %tmpf% %outf% frag_light_line

%glslc_exe% -fshader-stage=fragment -DUSE_LINE -DUSE_FOG light_frag.tmpl -o %tmpf%
%bh% %tmpf% %outf% frag_light_line_fog

rem compile generic shader variations from templates

rem single-texture vertex

%glslc_exe% -fshader-stage=vertex gen_vert.tmpl -o %tmpf%
%bh% %tmpf% %outf% vert_tx0

%glslc_exe% -fshader-stage=vertex -DUSE_FOG gen_vert.tmpl -o %tmpf%
%bh% %tmpf% %outf% vert_tx0_fog

%glslc_exe% -fshader-stage=vertex -DUSE_ENV gen_vert.tmpl -o %tmpf%
%bh% %tmpf% %outf% vert_tx0_env

%glslc_exe% -fshader-stage=vertex -DUSE_FOG -DUSE_ENV gen_vert.tmpl -o %tmpf%
%bh% %tmpf% %outf% vert_tx0_env_fog

%glslc_exe% -fshader-stage=vertex -DUSE_CL0_IDENT gen_vert.tmpl -o %tmpf%
%bh% %tmpf% %outf% vert_tx0_ident

rem double-texture vertex

%glslc_exe% -fshader-stage=vertex -DUSE_TX1 gen_vert.tmpl -o %tmpf%
%bh% %tmpf% %outf% vert_tx1

%glslc_exe% -fshader-stage=vertex -DUSE_TX1 -DUSE_FOG gen_vert.tmpl -o %tmpf%
%bh% %tmpf% %outf% vert_tx1_fog

%glslc_exe% -fshader-stage=vertex -DUSE_TX1 -DUSE_ENV gen_vert.tmpl -o %tmpf%
%bh% %tmpf% %outf% vert_tx1_env

%glslc_exe% -fshader-stage=vertex -DUSE_TX1 -DUSE_FOG -DUSE_ENV gen_vert.tmpl -o %tmpf%
%bh% %tmpf% %outf% vert_tx1_env_fog

rem double-texture vertex, non-identical colors

%glslc_exe% -fshader-stage=vertex -DUSE_CL1 -DUSE_TX1 gen_vert.tmpl -o %tmpf%
%bh% %tmpf% %outf% vert_tx1_cl

%glslc_exe% -fshader-stage=vertex -DUSE_CL1 -DUSE_TX1 -DUSE_FOG gen_vert.tmpl -o %tmpf%
%bh% %tmpf% %outf% vert_tx1_cl_fog

%glslc_exe% -fshader-stage=vertex -DUSE_CL1 -DUSE_TX1 -DUSE_ENV gen_vert.tmpl -o %tmpf%
%bh% %tmpf% %outf% vert_tx1_cl_env

%glslc_exe% -fshader-stage=vertex -DUSE_CL1 -DUSE_TX1 -DUSE_ENV -DUSE_FOG gen_vert.tmpl -o %tmpf%
%bh% %tmpf% %outf% vert_tx1_cl_env_fog

rem triple-texture vertex

%glslc_exe% -fshader-stage=vertex -DUSE_TX2 gen_vert.tmpl -o %tmpf%
%bh% %tmpf% %outf% vert_tx2

%glslc_exe% -fshader-stage=vertex -DUSE_TX2 -DUSE_FOG gen_vert.tmpl -o %tmpf%
%bh% %tmpf% %outf% vert_tx2_fog

%glslc_exe% -fshader-stage=vertex -DUSE_TX2 -DUSE_ENV gen_vert.tmpl -o %tmpf%
%bh% %tmpf% %outf% vert_tx2_env

%glslc_exe% -fshader-stage=vertex -DUSE_TX2 -DUSE_ENV -DUSE_FOG gen_vert.tmpl -o %tmpf%
%bh% %tmpf% %outf% vert_tx2_env_fog

rem triple-texture vertex, non-identical colors

%glslc_exe% -fshader-stage=vertex -DUSE_CL2 -DUSE_TX2 gen_vert.tmpl -o %tmpf%
%bh% %tmpf% %outf% vert_tx2_cl

%glslc_exe% -fshader-stage=vertex -DUSE_CL2 -DUSE_TX2 -DUSE_FOG gen_vert.tmpl -o %tmpf%
%bh% %tmpf% %outf% vert_tx2_cl_fog

%glslc_exe% -fshader-stage=vertex -DUSE_CL2 -DUSE_TX2 -DUSE_ENV gen_vert.tmpl -o %tmpf%
%bh% %tmpf% %outf% vert_tx2_cl_env

%glslc_exe% -fshader-stage=vertex -DUSE_CL2 -DUSE_TX2 -DUSE_ENV -DUSE_FOG gen_vert.tmpl -o %tmpf%
%bh% %tmpf% %outf% vert_tx2_cl_env_fog

rem single-texture fragment

%glslc_exe% -fshader-stage=fragment -DUSE_CL0_IDENT gen_frag.tmpl -o %tmpf%
%bh% %tmpf% %outf% frag_tx0_ident

%glslc_exe% -fshader-stage=fragment -DUSE_ATEST -DUSE_DF gen_frag.tmpl -o %tmpf%
%bh% %tmpf% %outf% frag_tx0_df

%glslc_exe% -fshader-stage=fragment -DUSE_ATEST gen_frag.tmpl -o %tmpf%
%bh% %tmpf% %outf% frag_tx0

%glslc_exe% -fshader-stage=fragment -DUSE_ATEST -DUSE_FOG gen_frag.tmpl -o %tmpf%
%bh% %tmpf% %outf% frag_tx0_fog

rem double-texture fragment

%glslc_exe% -fshader-stage=fragment -DUSE_TX1 gen_frag.tmpl -o %tmpf%
%bh% %tmpf% %outf% frag_tx1

%glslc_exe% -fshader-stage=fragment -DUSE_TX1 -DUSE_FOG gen_frag.tmpl -o %tmpf%
%bh% %tmpf% %outf% frag_tx1_fog

rem double-texture fragment, non-identical colors

%glslc_exe% -fshader-stage=fragment -DUSE_CL1 -DUSE_TX1 gen_frag.tmpl -o %tmpf%
%bh% %tmpf% %outf% frag_tx1_cl

%glslc_exe% -fshader-stage=fragment -DUSE_CL1 -DUSE_TX1 -DUSE_FOG gen_frag.tmpl -o %tmpf%
%bh% %tmpf% %outf% frag_tx1_cl_fog

rem triple-texture fragment

%glslc_exe% -fshader-stage=fragment -DUSE_TX2 gen_frag.tmpl -o %tmpf%
%bh% %tmpf% %outf% frag_tx2

%glslc_exe% -fshader-stage=fragment -DUSE_TX2 -DUSE_FOG gen_frag.tmpl -o %tmpf%
%bh% %tmpf% %outf% frag_tx2_fog

rem triple-texture fragment, non-identical colors

%glslc_exe% -fshader-stage=fragment -DUSE_CL2 -DUSE_TX2 gen_frag.tmpl -o %tmpf%
%bh% %tmpf% %outf% frag_tx2_cl

%glslc_exe% -fshader-stage=fragment -DUSE_CL2 -DUSE_TX2 -DUSE_FOG gen_frag.tmpl -o %tmpf%
%bh% %tmpf% %outf% frag_tx2_cl_fog

del /Q %tmpf%

pause
