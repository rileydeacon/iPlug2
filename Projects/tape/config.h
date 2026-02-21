#define PLUG_NAME "tape"
#define PLUG_MFR "curio machines"
#define PLUG_VERSION_HEX 0x00010100
#define PLUG_VERSION_STR "1.1.0"
#define PLUG_UNIQUE_ID 'tape'
#define PLUG_MFR_ID 'Curi'
#define PLUG_URL_STR "https://iplug2.github.io"
#define PLUG_EMAIL_STR "spam@me.com"
#define PLUG_COPYRIGHT_STR "Copyright 2025 Acme Inc"
#define PLUG_CLASS_NAME tape

#define BUNDLE_NAME "tape"
#define BUNDLE_MFR "curiomachines"
#define BUNDLE_DOMAIN "com"

#define SHARED_RESOURCES_SUBPATH "tape"

#define PLUG_CHANNEL_IO "1-1 2-2"

#define PLUG_LATENCY 0
#define PLUG_TYPE 0
#define PLUG_DOES_MIDI_IN 0
#define PLUG_DOES_MIDI_OUT 0
#define PLUG_DOES_MPE 0
#define PLUG_DOES_STATE_CHUNKS 1
#define PLUG_HAS_UI 1
#define PLUG_WIDTH 300
#define PLUG_HEIGHT 300
#define PLUG_FPS 60
#define PLUG_SHARED_RESOURCES 0
#define PLUG_HOST_RESIZE 0

// Curio Machines SVG / Bitmap UI Assets
#define BACKGROUND_FN "bg.png"
#define TAPE_KNOB_FN "tape-knob.svg"

#define AUV2_ENTRY tape_Entry
#define AUV2_ENTRY_STR "tape_Entry"
#define AUV2_FACTORY tape_Factory
#define AUV2_VIEW_CLASS tape_View
#define AUV2_VIEW_CLASS_STR "tape_View"

#define AAX_TYPE_IDS 'IEF1', 'IEF2'
#define AAX_TYPE_IDS_AUDIOSUITE 'IEA1', 'IEA2'
#define AAX_PLUG_MFR_STR "curio machines"
#define AAX_PLUG_NAME_STR "tape\nIPEF"
#define AAX_PLUG_CATEGORY_STR "Effect"
#define AAX_DOES_AUDIOSUITE 1

#define VST3_SUBCATEGORY "Fx"

#define CLAP_MANUAL_URL "https://iplug2.github.io/manuals/example_manual.pdf"
#define CLAP_SUPPORT_URL "https://github.com/iPlug2/iPlug2/wiki"
#define CLAP_DESCRIPTION "A simple audio effect for modifying gain"
#define CLAP_FEATURES "audio-effect" //, "utility"

#define APP_NUM_CHANNELS 2
#define APP_N_VECTOR_WAIT 0
#define APP_MULT 1
#define APP_COPY_AUV3 0
#define APP_SIGNAL_VECTOR_SIZE 64

#define ROBOTO_FN "Roboto-Regular.ttf"
