cmake_minimum_required(VERSION 3.21)

project(Nokturn VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Quick QuickControls2 Multimedia Concurrent Network WebSockets)
find_package(TagLib 2 REQUIRED)
if(UNIX AND NOT APPLE)
    find_package(Qt6 6.8 REQUIRED COMPONENTS DBus)   # MPRIS (klawisze multimedialne, panel systemowy)
endif()

# Identyfikator aplikacji Discord (discord.com/developers) dla statusu "Słucha …".
# Pusty = użytkownik wpisuje własny w Ustawieniach.
set(NOKTURN_DISCORD_CLIENT_ID "" CACHE STRING "Application ID aplikacji Discord dla Rich Presence")

qt_standard_project_setup(REQUIRES 6.8)
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)   # qmldir w podkatalogach qml/components, qml/layout, qml/pages
endif()

qt_add_executable(Nokturn
    src/main.cpp
)

# Singletons zdefiniowane w QML
set_source_files_properties(
    qml/Theme.qml
    qml/Icons.qml
    qml/Nav.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(Nokturn
    URI Nokturn
    VERSION 1.0
    SOURCES
        src/AppPaths.h
        src/TagLibPath.h
        src/Prefs.h             src/Prefs.cpp
        src/TrackModel.h        src/TrackModel.cpp
        src/LibraryScanner.h    src/LibraryScanner.cpp
        src/SpectrumAnalyzer.h  src/SpectrumAnalyzer.cpp
        src/Library.h           src/Library.cpp
        src/PlayerController.h  src/PlayerController.cpp
        src/audio/Equalizer.h         src/audio/Equalizer.cpp
        src/audio/AudioEngine.h       src/audio/AudioEngine.cpp
        src/youtube/YtCookies.h       src/youtube/YtCookies.cpp
        src/youtube/BrowserLogin.h    src/youtube/BrowserLogin.cpp
        src/youtube/YtMusicApi.h      src/youtube/YtMusicApi.cpp
        src/youtube/StreamResolver.h  src/youtube/StreamResolver.cpp
        src/youtube/YouTubeService.h  src/youtube/YouTubeService.cpp
        src/lyrics/Lyrics.h           src/lyrics/Lyrics.cpp
        src/lyrics/LyricsAligner.h    src/lyrics/LyricsAligner.cpp
        src/lyrics/SpeechEngine.h     src/lyrics/SpeechEngine.cpp
        src/lyrics/LyricsService.h    src/lyrics/LyricsService.cpp
        src/platform/DiscordPresence.h src/platform/DiscordPresence.cpp
        src/Updater.h           src/Updater.cpp
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/Icons.qml
        qml/Nav.qml
        qml/DevTour.qml

        qml/components/AmbientBackground.qml
        qml/components/AlbumCard.qml
        qml/components/ArtCover.qml
        qml/components/EqGraph.qml
        qml/components/EqualizerIcon.qml
        qml/components/Fader.qml
        qml/components/HeroCard.qml
        qml/components/Icon.qml
        qml/components/IconButton.qml
        qml/components/InputField.qml
        qml/components/LyricsView.qml
        qml/components/LogoMark.qml
        qml/components/PageFlick.qml
        qml/components/Panel.qml
        qml/components/PillButton.qml
        qml/components/PlayButton.qml
        qml/components/PlayFab.qml
        qml/components/ProgressSlider.qml
        qml/components/QuickTile.qml
        qml/components/Reveal.qml
        qml/components/SectionHeader.qml
        qml/components/SegmentedTabs.qml
        qml/components/SettingRow.qml
        qml/components/SourceBadge.qml
        qml/components/SourceCard.qml
        qml/components/Spinner.qml
        qml/components/StatusPill.qml
        qml/components/ThinScrollBar.qml
        qml/components/Toast.qml
        qml/components/Toggle.qml
        qml/components/UpdateDialog.qml
        qml/components/TrackListHeader.qml
        qml/components/TrackRow.qml
        qml/components/Visualizer.qml
        qml/components/WindowControls.qml
        qml/components/WindowResizer.qml

        qml/layout/TitleBar.qml
        qml/layout/Sidebar.qml
        qml/layout/SidebarItem.qml
        qml/layout/PlayerBar.qml
        qml/layout/NowPlayingPanel.qml
        qml/layout/ImmersiveView.qml

        qml/pages/HomePage.qml
        qml/pages/SearchPage.qml
        qml/pages/EqualizerPage.qml
        qml/pages/LibraryPage.qml
        qml/pages/PlaylistPage.qml
        qml/pages/SourcesPage.qml
        qml/pages/SettingsPage.qml

        qml/onboarding/Onboarding.qml
        qml/onboarding/LayoutPreview.qml
        qml/onboarding/CrossfadePreview.qml
        qml/onboarding/OptionCard.qml
        qml/onboarding/StepTitle.qml
        qml/onboarding/GroupLabel.qml
        qml/onboarding/StepCard.qml
        qml/onboarding/WelcomeStep.qml
        qml/onboarding/LayoutStep.qml
        qml/onboarding/ColorsStep.qml
        qml/onboarding/MusicStep.qml
        qml/onboarding/PlaybackStep.qml
        qml/onboarding/IntegrationsStep.qml
        qml/onboarding/DoneStep.qml
)

set_target_properties(Nokturn PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

# Na Linuksie program nazywa się "nokturn" (konwencja + brak kolizji z katalogiem modułu QML "Nokturn").
if(UNIX AND NOT APPLE)
    set_target_properties(Nokturn PROPERTIES OUTPUT_NAME nokturn)
endif()

target_include_directories(Nokturn PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/src/youtube ${CMAKE_CURRENT_SOURCE_DIR}/src/audio ${CMAKE_CURRENT_SOURCE_DIR}/src/lyrics ${CMAKE_CURRENT_SOURCE_DIR}/src/platform)
target_link_libraries(Nokturn PRIVATE
    Qt6::Quick Qt6::QuickControls2 Qt6::Multimedia Qt6::Concurrent Qt6::Network Qt6::WebSockets
    TagLib::tag
    $<$<PLATFORM_ID:Windows>:crypt32>
)
target_compile_definitions(Nokturn PRIVATE
    NOKTURN_DISCORD_CLIENT_ID="${NOKTURN_DISCORD_CLIENT_ID}"
    NOKTURN_VERSION="${PROJECT_VERSION}")

if(UNIX AND NOT APPLE)
    target_sources(Nokturn PRIVATE src/platform/Mpris.h src/platform/Mpris.cpp)
    target_link_libraries(Nokturn PRIVATE Qt6::DBus)
    target_compile_definitions(Nokturn PRIVATE NOKTURN_MPRIS)
endif()

if(WIN32)
    # System Media Transport Controls (okienko multimediów Windows) + klawisze multimedialne
    target_sources(Nokturn PRIVATE
        src/platform/WinRtMedia.h
        src/platform/WindowsMediaControls.h src/platform/WindowsMediaControls.cpp)
    target_link_libraries(Nokturn PRIVATE runtimeobject shlwapi shcore)
    target_compile_definitions(Nokturn PRIVATE NOKTURN_SMTC)
endif()

if(WIN32)
    install(TARGETS Nokturn RUNTIME DESTINATION .)
else()
    include(GNUInstallDirs)
    install(TARGETS Nokturn RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES packaging/linux/nokturn.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES packaging/linux/nokturn.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
    install(FILES packaging/linux/nokturn.metainfo.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
endif()
