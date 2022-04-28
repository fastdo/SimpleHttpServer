for %%f in (%FASTDO_X64D_BIN%\*.dll) do (
    @echo ---- mklink ----
    @mklink %%~nxf %%f
)
