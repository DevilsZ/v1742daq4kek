#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#pragma pack(push, 1)  

struct EventHeader {
    uint32_t marker_begin;      // 0xAAAA1111 等の固定値
    uint32_t header_size;       // この構造体自体のサイズ
    uint32_t payload_size;      // 後続する全ボードデータの合計サイズ
    uint64_t event_id;          // 連番
    uint64_t sec;               // タイムスタンプ（秒）
    uint64_t nsec;              // タイムスタンプ（ナノ秒）
    uint32_t trigger_counter;   // ボードがカウントしたトリガー数
    uint32_t flags;             // エラーフラグ等
    uint32_t spare;             // 将来の拡張用
};

struct BoardHeader {
    uint32_t marker_begin;      // 0xBBBB1111 等
    uint32_t header_size;       // この構造体自体のサイズ
    uint32_t payload_size;      // このボードの純粋なデータサイズ
    int board_id;               // 0, 1, 2...
};

struct EventTrailer {
    uint32_t checksum;          // データ整合性チェック用（任意）
    uint32_t marker_end;        // 0xEEEE2222 等
};

#pragma pack(pop) 

#endif
