add_subdirectory(qtexttospeech)
if(TARGET Qt::Qml AND TARGET Qt::QuickTest)
    add_subdirectory(qtexttospeech_qml)
endif()
add_subdirectory(qvoice)
