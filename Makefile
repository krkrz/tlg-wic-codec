NAME = tlg-wic-codec
SRCS = Release-Win32/${NAME}.dll Release-x64/${NAME}.dll README.md
YMD = $(shell date "+%Y%m%d")
arc:
	zip "${NAME}${YMD}.zip" ${SRCS}
