#include "_protocols.h"

const Protocol* protocols[] = {
    &protocol_continuity,
    &protocol_easysetup,
    &protocol_fastpair,
    &protocol_lovespouse,
    &protocol_swiftpair,
    &protocol_fastpair_mt,
};

const size_t protocols_count = COUNT_OF(protocols);
