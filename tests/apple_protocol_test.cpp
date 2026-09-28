#include "ersa/protocols/apple_notifications.h"
#include "ersa/protocols/apple_media.h"
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <string.h>

using namespace ersa::protocols;

void test_apple_protocols() {
    const uint32_t uid = 0x89abcdef;
    std::vector<uint8_t> response = {0, 0xef, 0xcd, 0xab, 0x89, 1, 5, 0, 'A','l','i','c','e',
                                   3, 5, 0, 'H','e','l','l','o'};
    // Split at every header/length/value boundary (including single-byte fragments).
    for (size_t split = 1; split < response.size(); ++split) {
        AncsAttributes parser;
        parser.begin(uid);
        assert(parser.feed(response.data(), split) == AncsAttributes::Result::More);
        assert(parser.feed(response.data() + split, response.size() - split) == AncsAttributes::Result::Complete);
        assert(strcmp(parser.title, "Alice") == 0 && strcmp(parser.message, "Hello") == 0);
    }
    AncsAttributes parser;
    parser.begin(uid);
    for (size_t i = 0; i < response.size(); ++i)
        assert(parser.feed(&response[i], 1) == (i + 1 == response.size() ? AncsAttributes::Result::Complete : AncsAttributes::Result::More));

    uint8_t request[11];
    AncsAttributes::request(uid, request);
    const uint8_t expected[] = {0, 0xef, 0xcd, 0xab, 0x89, 1, 31, 0, 3, 63, 0};
    assert(memcmp(request, expected, sizeof(request)) == 0);

    const uint8_t dismissalResponse[] = {0, 0xef, 0xcd, 0xab, 0x89,
        1, 5, 0, 'A','l','i','c','e', 3, 5, 0, 'H','e','l','l','o',
        7, 5, 0, 'C','l','e','a','r'};
    uint8_t actionRequest[14];
    AncsAttributes::request(uid, true, actionRequest);
    const uint8_t expectedActionRequest[] = {0, 0xef, 0xcd, 0xab, 0x89,
        1, 31, 0, 3, 63, 0, 7, 31, 0};
    assert(memcmp(actionRequest, expectedActionRequest, sizeof(actionRequest)) == 0);
    parser.begin(uid, true);
    for (size_t i = 0; i < sizeof(dismissalResponse); ++i)
        assert(parser.feed(dismissalResponse + i, 1) ==
            (i + 1 == sizeof(dismissalResponse) ? AncsAttributes::Result::Complete : AncsAttributes::Result::More));
    assert(parser.negativeActionIsDismissal());
    const uint8_t destructiveResponse[] = {0, 0xef, 0xcd, 0xab, 0x89,
        1, 5, 0, 'A','l','i','c','e', 3, 5, 0, 'H','e','l','l','o',
        7, 6, 0, 'D','e','l','e','t','e'};
    parser.begin(uid, true);
    assert(parser.feed(destructiveResponse, sizeof(destructiveResponse)) == AncsAttributes::Result::Complete);
    assert(!parser.negativeActionIsDismissal());
    parser.begin(uid + 1);
    assert(parser.feed(response.data(), response.size()) == AncsAttributes::Result::Invalid);
    parser.begin(uid);
    auto invalid = response;
    invalid[6] = 0xff; invalid[7] = 0xff;
    assert(parser.feed(invalid.data(), invalid.size()) == AncsAttributes::Result::Invalid);
    parser.begin(uid);
    invalid = response; invalid.push_back(0);
    assert(parser.feed(invalid.data(), invalid.size()) == AncsAttributes::Result::Invalid);

    // Missing/empty attributes still complete and must not reuse previous text.
    const uint8_t empty[] = {0, 0, 0, 0, 0, 1, 0, 0, 3, 0, 0};
    parser.begin(0);
    assert(parser.feed(empty, sizeof(empty)) == AncsAttributes::Result::Complete);
    assert(!parser.title[0] && !parser.message[0]);
    std::vector<uint8_t> maximum = {0, 0, 0, 0, 0, 1, 31, 0};
    maximum.insert(maximum.end(), 31, 't');
    maximum.insert(maximum.end(), {3, 63, 0});
    maximum.insert(maximum.end(), 63, 'm');
    parser.begin(0);
    assert(parser.feed(maximum.data(), maximum.size()) == AncsAttributes::Result::Complete);
    assert(strlen(parser.title) == 31 && strlen(parser.message) == 63);

    AncsCall call;
    uint8_t action[6];
    call.update(uid, 8 | 16);
    assert(call.action(uid, true, action));
    assert(action[0] == 2 && readLe32(action + 1) == uid && action[5] == 0);
    assert(!call.remove(uid + 1)); // unrelated message removal cannot end ringing
    assert(call.action(uid, false, action) && action[5] == 1);
    call.update(uid, 16);
    assert(!call.action(uid, true, action)); // iOS revoked positive action
    assert(call.remove(uid));
    assert(!call.action(uid, false, action)); // removed UID is no longer actionable
    call.update(uid + 1, 24);
    assert(!call.action(uid, true, action)); // button queued for previous call

    AmsMedia media;
    const uint8_t title[] = {2, 2, 0, 'S','o','n','g'};
    const uint8_t artist[] = {2, 0, 0, 'A'};
    const uint8_t play[] = {0, 1, 0, '1', ',', '1', ',', '0'};
    assert(media.update(title, sizeof(title)) && strcmp(media.title, "Song") == 0);
    assert(media.update(artist, sizeof(artist)) && strcmp(media.artist, "A") == 0);
    assert(media.update(play, sizeof(play)) && media.playing);
    const uint8_t emptyTitle[] = {2, 2, 0};
    assert(media.update(emptyTitle, sizeof(emptyTitle)) && !media.title[0]);
    assert(!media.update(play, 2));
    const uint8_t malformed[] = {0, 1, 0, 'x', ','};
    assert(!media.update(malformed, sizeof(malformed)));
    puts("PASS: apple protocol fragmentation, UID lifetime and media decoding");
}
