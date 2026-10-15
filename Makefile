## Haiku Generic Makefile v2.6 ##
NAME = Quill
TYPE = APP
APP_MIME_SIG = application/x-vnd.Quill

SRCS = \
	src/core/Format.cpp \
	src/core/Text.cpp \
	src/core/Document.cpp \
	src/core/Editor.cpp \
	src/core/Layout.cpp \
	src/core/RTF.cpp \
	src/core/TextIO.cpp \
	src/core/Search.cpp \
	src/core/Tools.cpp \
	src/core/Spell.cpp \
	src/app/App.cpp \
	src/app/Bars.cpp \
	src/app/Controls.cpp \
	src/app/DocView.cpp \
	src/app/DocWindow.cpp \
	src/app/Fonts.cpp \
	src/app/FormatDialogs.cpp \
	src/app/FormatMessages.cpp \
	src/app/Icons.cpp \
	src/app/PageRenderer.cpp \
	src/app/Painter.cpp \
	src/app/Panes.cpp \
	src/app/Prefs.cpp \
	src/app/ToolDialogs.cpp

RDEFS = Quill.rdef
LIBS = be tracker translation $(STDCPPLIBS)
LOCAL_INCLUDE_PATHS = src/core src/app
OPTIMIZE := FULL
WARNINGS = ALL
COMPILER_FLAGS =
DEBUGGER :=
DEVEL_DIRECTORY := /boot/system/develop
include $(DEVEL_DIRECTORY)/etc/makefile-engine
