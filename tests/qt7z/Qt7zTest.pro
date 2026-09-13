#-------------------------------------------------
#
# Project created by QtCreator 2014-02-08T20:25:29
#
#-------------------------------------------------

QT       += testlib

TARGET = Qt7zTest
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

include("../../components/qt7z/Qt7z.pri")

SOURCES += Qt7zPackage_Tests.cpp
DEFINES += SRCDIR=\\\"$$PWD/\\\"

RESOURCES += Qt7zTest.qrc

assets.files = $$PWD/assets/*
assets.path = $$OUT_PWD/assets

INSTALLS += assets
