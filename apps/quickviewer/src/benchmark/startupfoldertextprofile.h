#ifndef STARTUPFOLDERTEXTPROFILE_H
#define STARTUPFOLDERTEXTPROFILE_H

#include <functional>

class FolderItemModel;
class QWidget;

/**
 * Measurement only: drives the folder-text startup profile described in
 * developer/Testing.md. MainWindow starts it when the startup profiler is
 * enabled and QV_PROFILE_FOLDER_TEXT is set, so a normal start never reaches it.
 */
class StartupFolderTextProfile
{
public:
    using ModelLookup = std::function<FolderItemModel *()>;
    static void watch(QWidget *window, ModelLookup model);
};

#endif // STARTUPFOLDERTEXTPROFILE_H
