for %%f in (%FASTDO_X64R_BIN%\*.dll) do (
    @echo ---- mklink ----
    @mklink %%~nxf %%f
)
