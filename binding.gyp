{
    "targets": [
        {
            "target_name": "capture",
            "sources": ["capture.cc"],
            "libraries": ["-lasound -lm"],
            "cflags": ["-Wall", "-std=c++17"],
            "cflags_cc": ["-Wall", "-std=c++17"],
            "cflags!": ["-fno-exceptions"],
            "cflags_cc!": ["-fno-exceptions"],
            "include_dirs": ["<!(node -e \"require('nan')\")"],
        }
    ]
}
