    // probonopd/irdb television sheets. Only protocols whose command byte matches a code already verified here.

    // codes__Emerson__TV__0,-1.csv
    {"Emerson 2", "NEC",
     {
        {PROTO_NEC, 32, 0x0, 0x17}, // MUTE
        {PROTO_NEC, 32, 0x0, 0x13}, // VOL+
        {PROTO_NEC, 32, 0x0, 0xF}, // VOL-
        {PROTO_NEC, 32, 0x0, 0x1F}, // POWER
        {PROTO_NEC, 32, 0x0, 0xB}, // CH+
        {PROTO_NEC, 32, 0x0, 0x7}, // CH-
        {0, 0, 0, 0}, // INPUT
        {0, 0, 0, 0}, // OK
        {0, 0, 0, 0}, // UP
        {0, 0, 0, 0}, // DOWN
        {0, 0, 0, 0}, // LEFT
        {0, 0, 0, 0}, // RIGHT
        {0, 0, 0, 0}, // BACK
        {0, 0, 0, 0}, // MENU
     }},
    // codes__Hitachi__TV__12,251.csv
    {"Hitachi 3", "NEC",
     {
        {0, 0, 0, 0}, // MUTE
        {0, 0, 0, 0}, // VOL+
        {0, 0, 0, 0}, // VOL-
        {PROTO_NEC, 32, 0xFB0C, 0x95}, // POWER
        {0, 0, 0, 0}, // CH+
        {0, 0, 0, 0}, // CH-
        {PROTO_NEC, 32, 0xFB0C, 0x81}, // INPUT
        {0, 0, 0, 0}, // OK
        {PROTO_NEC, 32, 0xFB0C, 0x99}, // UP
        {PROTO_NEC, 32, 0xFB0C, 0x90}, // DOWN
        {PROTO_NEC, 32, 0xFB0C, 0x94}, // LEFT
        {PROTO_NEC, 32, 0xFB0C, 0x97}, // RIGHT
        {PROTO_NEC, 32, 0xFB0C, 0x8F}, // BACK
        {PROTO_NEC, 32, 0xFB0C, 0xF0}, // MENU
     }},
    // codes__Hitachi__TV__80,173.csv
    {"Hitachi 4", "NEC",
     {
        {0, 0, 0, 0}, // MUTE
        {0, 0, 0, 0}, // VOL+
        {0, 0, 0, 0}, // VOL-
        {PROTO_NEC, 32, 0xAD50, 0x1}, // POWER
        {0, 0, 0, 0}, // CH+
        {0, 0, 0, 0}, // CH-
        {0, 0, 0, 0}, // INPUT
        {PROTO_NEC, 32, 0xAD50, 0x18}, // OK
        {PROTO_NEC, 32, 0xAD50, 0x14}, // UP
        {PROTO_NEC, 32, 0xAD50, 0x15}, // DOWN
        {PROTO_NEC, 32, 0xAD50, 0x17}, // LEFT
        {PROTO_NEC, 32, 0xAD50, 0x16}, // RIGHT
        {PROTO_NEC, 32, 0xAD50, 0x1A}, // BACK
        {PROTO_NEC, 32, 0xAD50, 0x13}, // MENU
     }},
    // codes__JVC__TV__3,-1.csv
    {"JVC 2", "JVC",
     {
        {PROTO_JVC, 16, 0x3, 0x1C}, // MUTE
        {PROTO_JVC, 16, 0x3, 0x1E}, // VOL+
        {PROTO_JVC, 16, 0x3, 0x1F}, // VOL-
        {PROTO_JVC, 16, 0x3, 0x17}, // POWER
        {PROTO_JVC, 16, 0x3, 0x19}, // CH+
        {PROTO_JVC, 16, 0x3, 0x18}, // CH-
        {PROTO_JVC, 16, 0x3, 0x13}, // INPUT
        {PROTO_JVC, 16, 0x3, 0xA}, // OK
        {PROTO_JVC, 16, 0x3, 0x7C}, // UP
        {PROTO_JVC, 16, 0x3, 0x7B}, // DOWN
        {PROTO_JVC, 16, 0x3, 0x5B}, // LEFT
        {PROTO_JVC, 16, 0x3, 0x5A}, // RIGHT
        {PROTO_JVC, 16, 0x3, 0x5}, // BACK
        {PROTO_JVC, 16, 0x3, 0x7A}, // MENU
     }},
    // codes__JVC__TV__31,-1.csv
    {"JVC 3", "JVC",
     {
        {0, 0, 0, 0}, // MUTE
        {0, 0, 0, 0}, // VOL+
        {0, 0, 0, 0}, // VOL-
        {PROTO_JVC, 16, 0x1F, 0xDD}, // POWER
        {0, 0, 0, 0}, // CH+
        {0, 0, 0, 0}, // CH-
        {0, 0, 0, 0}, // INPUT
        {0, 0, 0, 0}, // OK
        {0, 0, 0, 0}, // UP
        {0, 0, 0, 0}, // DOWN
        {0, 0, 0, 0}, // LEFT
        {0, 0, 0, 0}, // RIGHT
        {0, 0, 0, 0}, // BACK
        {0, 0, 0, 0}, // MENU
     }},
    // codes__LG__TV__1,1.csv
    {"LG 4", "NEC",
     {
        {PROTO_NEC, 32, 0x101, 0xA}, // MUTE
        {0, 0, 0, 0}, // VOL+
        {0, 0, 0, 0}, // VOL-
        {PROTO_NEC, 32, 0x101, 0x1C}, // POWER
        {0, 0, 0, 0}, // CH+
        {0, 0, 0, 0}, // CH-
        {0, 0, 0, 0}, // INPUT
        {0, 0, 0, 0}, // OK
        {0, 0, 0, 0}, // UP
        {0, 0, 0, 0}, // DOWN
        {0, 0, 0, 0}, // LEFT
        {0, 0, 0, 0}, // RIGHT
        {0, 0, 0, 0}, // BACK
        {0, 0, 0, 0}, // MENU
     }},
    // codes__Marantz__TV__24,-1.csv
    {"Marantz", "NEC",
     {
        {PROTO_NEC, 32, 0x18, 0x9}, // MUTE
        {0, 0, 0, 0}, // VOL+
        {0, 0, 0, 0}, // VOL-
        {PROTO_NEC, 32, 0x18, 0x8}, // POWER
        {0, 0, 0, 0}, // CH+
        {0, 0, 0, 0}, // CH-
        {0, 0, 0, 0}, // INPUT
        {0, 0, 0, 0}, // OK
        {0, 0, 0, 0}, // UP
        {0, 0, 0, 0}, // DOWN
        {0, 0, 0, 0}, // LEFT
        {0, 0, 0, 0}, // RIGHT
        {0, 0, 0, 0}, // BACK
        {0, 0, 0, 0}, // MENU
     }},
    // codes__Marantz__TV__24,247.csv
    {"Marantz 2", "NEC",
     {
        {0, 0, 0, 0}, // MUTE
        {0, 0, 0, 0}, // VOL+
        {0, 0, 0, 0}, // VOL-
        {PROTO_NEC, 32, 0xF718, 0x8}, // POWER
        {0, 0, 0, 0}, // CH+
        {0, 0, 0, 0}, // CH-
        {0, 0, 0, 0}, // INPUT
        {0, 0, 0, 0}, // OK
        {0, 0, 0, 0}, // UP
        {0, 0, 0, 0}, // DOWN
        {0, 0, 0, 0}, // LEFT
        {0, 0, 0, 0}, // RIGHT
        {0, 0, 0, 0}, // BACK
        {0, 0, 0, 0}, // MENU
     }},
    // codes__Mitsubishi__TV__1,-1.csv
    {"Sharp 3", "SHARP",
     {
        {PROTO_SHARP, 15, 0x1, 0x17}, // MUTE
        {PROTO_SHARP, 15, 0x1, 0x14}, // VOL+
        {PROTO_SHARP, 15, 0x1, 0x15}, // VOL-
        {PROTO_SHARP, 15, 0x1, 0x16}, // POWER
        {PROTO_SHARP, 15, 0x1, 0x11}, // CH+
        {PROTO_SHARP, 15, 0x1, 0x12}, // CH-
        {PROTO_SHARP, 15, 0x1, 0x13}, // INPUT
        {0, 0, 0, 0}, // OK
        {0, 0, 0, 0}, // UP
        {0, 0, 0, 0}, // DOWN
        {0, 0, 0, 0}, // LEFT
        {0, 0, 0, 0}, // RIGHT
        {0, 0, 0, 0}, // BACK
        {PROTO_SHARP, 15, 0x1, 0x20}, // MENU
     }},
    // codes__Sony__TV__84,-1.csv
    {"Sony 3", "SIRC",
     {
        {0, 0, 0, 0}, // MUTE
        {0, 0, 0, 0}, // VOL+
        {0, 0, 0, 0}, // VOL-
        {PROTO_SONY, 15, 0x54, 0x2E}, // POWER
        {0, 0, 0, 0}, // CH+
        {0, 0, 0, 0}, // CH-
        {PROTO_SONY, 15, 0x54, 0x2A}, // INPUT
        {PROTO_SONY, 15, 0x54, 0x5A}, // OK
        {PROTO_SONY, 15, 0x54, 0x35}, // UP
        {PROTO_SONY, 15, 0x54, 0x36}, // DOWN
        {PROTO_SONY, 15, 0x54, 0x34}, // LEFT
        {PROTO_SONY, 15, 0x54, 0x33}, // RIGHT
        {0, 0, 0, 0}, // BACK
        {PROTO_SONY, 15, 0x54, 0x29}, // MENU
     }},
