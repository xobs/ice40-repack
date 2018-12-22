# Ice Repack

Repack a bitstream's BIOS image.

## Background

The ICE40 parts have built-in block RAM that can start out initialized.  It is possible to replace this RAM, but the trick is knowing where it is.

To accomplish this, ensure RAM is filled with random data when you initialize it.

## Technical

The block ram is 16 bits, so 32-bit words are striped across two `.ram_data` configuration blocks.  This program searches for matching BRAM blocks and patches them.

This program borrows from both `icebram` and `icepack` to perform its functions.