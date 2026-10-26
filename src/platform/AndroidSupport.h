#pragma once

#include <QString>
#include <QUrl>

#include <functional>

// Android specifics: the storage permission for the local library and the folder picker's URIs.
namespace AndroidSupport {

// Asks for READ_MEDIA_AUDIO (Android 13+) or READ_EXTERNAL_STORAGE once; `done` runs on the GUI thread.
void requestAudioPermission(std::function<void(bool granted)> done);

// The system folder picker returns a content:// tree URI; the scanner needs a file path.
// Folders on the shared storage map to /storage/emulated/0/…, other volumes to /storage/<id>/….
QString folderPath(const QUrl &url);

} // namespace AndroidSupport
