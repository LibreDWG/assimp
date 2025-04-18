/*
---------------------------------------------------------------------------
Open Asset Import Library (assimp)
---------------------------------------------------------------------------

Copyright (c) 2006-2025, assimp team

All rights reserved.

Redistribution and use of this software in source and binary forms,
with or without modification, are permitted provided that the following
conditions are met:

* Redistributions of source code must retain the above
  copyright notice, this list of conditions and the
  following disclaimer.

* Redistributions in binary form must reproduce the above
  copyright notice, this list of conditions and the
  following disclaimer in the documentation and/or other
  materials provided with the distribution.

* Neither the name of the assimp team, nor the names of its
  contributors may be used to endorse or promote products
  derived from this software without specific prior
  written permission of the assimp team.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
---------------------------------------------------------------------------
*/

/** @file  DXFLoader.cpp
 *  @brief Implementation of the DXF importer class
 */

#ifndef ASSIMP_BUILD_NO_DXF_IMPORTER

#include "DXFLoader.h"
#include "DXFHelper.h"
#include "PostProcessing/ConvertToLHProcess.h"

#include <assimp/ParsingUtils.h>
#include <assimp/fast_atof.h>
#include <assimp/IOSystem.hpp>
#include <assimp/scene.h>
#include <assimp/importerdesc.h>

#include <cstdlib>
#include <numeric>
#include <utility>

using namespace Assimp;

// AutoCAD Binary DXF<CR><LF><SUB><NULL>
static constexpr char AI_DXF_BINARY_IDENT[] = "AutoCAD Binary DXF\r\n\x1a";
static constexpr size_t AI_DXF_BINARY_IDENT_LEN = sizeof AI_DXF_BINARY_IDENT;

// default vertex color that all uncolored vertices will receive
static const aiColor4D AI_DXF_DEFAULT_COLOR(aiColor4D(0.6f, 0.6f, 0.6f, 0.6f));

// color indices for DXF - 256 are supported, the table is
// taken directly from the AutoCad Index (ACI) table
// https://gohtx.com/acadcolors.php
//STH 2024-0126
static const aiColor4D g_aclrDxfIndexColors[256] = {
    aiColor4D (0.0f, 0.0f ,0.0f, 1.0f), //dxf color code 0
    aiColor4D (1.0f, 0.0f ,0.0f, 1.0f), //dxf color code 1
    aiColor4D (1.0f, 1.0f ,0.0f, 1.0f), //dxf color code 2
    aiColor4D (0.0f, 1.0f ,0.0f, 1.0f), //dxf color code 3
    aiColor4D (0.0f, 1.0f ,1.0f, 1.0f), //dxf color code 4
    aiColor4D (0.0f, 0.0f ,1.0f, 1.0f), //dxf color code 5
    aiColor4D (1.0f, 0.0f ,1.0f, 1.0f), //dxf color code 6
    aiColor4D (1.0f, 1.0f ,1.0f, 1.0f), //dxf color code 7
    aiColor4D (0.3f, 0.3f ,0.3f, 1.0f), //dxf color code 8
    aiColor4D (0.5f, 0.5f ,0.5f, 1.0f), //dxf color code 9
    aiColor4D (1.0f, 0.0f ,0.0f, 1.0f), //dxf color code 10
    aiColor4D (1.0f, 0.7f ,0.7f, 1.0f), //dxf color code 11
    aiColor4D (0.7f, 0.0f ,0.0f, 1.0f), //dxf color code 12
    aiColor4D (0.7f, 0.5f ,0.5f, 1.0f), //dxf color code 13
    aiColor4D (0.5f, 0.0f ,0.0f, 1.0f), //dxf color code 14
    aiColor4D (0.5f, 0.3f ,0.3f, 1.0f), //dxf color code 15
    aiColor4D (0.4f, 0.0f ,0.0f, 1.0f), //dxf color code 16
    aiColor4D (0.4f, 0.3f ,0.3f, 1.0f), //dxf color code 17
    aiColor4D (0.3f, 0.0f ,0.0f, 1.0f), //dxf color code 18
    aiColor4D (0.3f, 0.2f ,0.2f, 1.0f), //dxf color code 19
    aiColor4D (1.0f, 0.2f ,0.0f, 1.0f), //dxf color code 20
    aiColor4D (1.0f, 0.7f ,0.7f, 1.0f), //dxf color code 21
    aiColor4D (0.7f, 0.2f ,0.0f, 1.0f), //dxf color code 22
    aiColor4D (0.7f, 0.6f ,0.5f, 1.0f), //dxf color code 23
    aiColor4D (0.5f, 0.1f ,0.0f, 1.0f), //dxf color code 24
    aiColor4D (0.5f, 0.4f ,0.3f, 1.0f), //dxf color code 25
    aiColor4D (0.4f, 0.1f ,0.0f, 1.0f), //dxf color code 26
    aiColor4D (0.4f, 0.3f ,0.3f, 1.0f), //dxf color code 27
    aiColor4D (0.3f, 0.1f ,0.0f, 1.0f), //dxf color code 28
    aiColor4D (0.3f, 0.2f ,0.2f, 1.0f), //dxf color code 29
    aiColor4D (1.0f, 0.5f ,0.0f, 1.0f), //dxf color code 30
    aiColor4D (1.0f, 0.8f ,0.7f, 1.0f), //dxf color code 31
    aiColor4D (0.7f, 0.4f ,0.0f, 1.0f), //dxf color code 32
    aiColor4D (0.7f, 0.6f ,0.5f, 1.0f), //dxf color code 33
    aiColor4D (0.5f, 0.3f ,0.0f, 1.0f), //dxf color code 34
    aiColor4D (0.5f, 0.4f ,0.3f, 1.0f), //dxf color code 35
    aiColor4D (0.4f, 0.2f ,0.0f, 1.0f), //dxf color code 36
    aiColor4D (0.4f, 0.3f ,0.3f, 1.0f), //dxf color code 37
    aiColor4D (0.3f, 0.2f ,0.0f, 1.0f), //dxf color code 38
    aiColor4D (0.3f, 0.3f ,0.2f, 1.0f), //dxf color code 39
    aiColor4D (1.0f, 0.7f ,0.0f, 1.0f), //dxf color code 40
    aiColor4D (1.0f, 0.9f ,0.7f, 1.0f), //dxf color code 41
    aiColor4D (0.7f, 0.6f ,0.0f, 1.0f), //dxf color code 42
    aiColor4D (0.7f, 0.7f ,0.5f, 1.0f), //dxf color code 43
    aiColor4D (0.5f, 0.4f ,0.0f, 1.0f), //dxf color code 44
    aiColor4D (0.5f, 0.5f ,0.3f, 1.0f), //dxf color code 45
    aiColor4D (0.4f, 0.3f ,0.0f, 1.0f), //dxf color code 46
    aiColor4D (0.4f, 0.4f ,0.3f, 1.0f), //dxf color code 47
    aiColor4D (0.3f, 0.2f ,0.0f, 1.0f), //dxf color code 48
    aiColor4D (0.3f, 0.3f ,0.2f, 1.0f), //dxf color code 49
    aiColor4D (1.0f, 1.0f ,0.0f, 1.0f), //dxf color code 50
    aiColor4D (1.0f, 1.0f ,0.7f, 1.0f), //dxf color code 51
    aiColor4D (0.7f, 0.7f ,0.0f, 1.0f), //dxf color code 52
    aiColor4D (0.7f, 0.7f ,0.5f, 1.0f), //dxf color code 53
    aiColor4D (0.5f, 0.5f ,0.0f, 1.0f), //dxf color code 54
    aiColor4D (0.5f, 0.5f ,0.3f, 1.0f), //dxf color code 55
    aiColor4D (0.4f, 0.4f ,0.0f, 1.0f), //dxf color code 56
    aiColor4D (0.4f, 0.4f ,0.3f, 1.0f), //dxf color code 57
    aiColor4D (0.3f, 0.3f ,0.0f, 1.0f), //dxf color code 58
    aiColor4D (0.3f, 0.3f ,0.2f, 1.0f), //dxf color code 59
    aiColor4D (0.7f, 1.0f ,0.0f, 1.0f), //dxf color code 60
    aiColor4D (0.9f, 1.0f ,0.7f, 1.0f), //dxf color code 61
    aiColor4D (0.6f, 0.7f ,0.0f, 1.0f), //dxf color code 62
    aiColor4D (0.7f, 0.7f ,0.5f, 1.0f), //dxf color code 63
    aiColor4D (0.4f, 0.5f ,0.0f, 1.0f), //dxf color code 64
    aiColor4D (0.5f, 0.5f ,0.3f, 1.0f), //dxf color code 65
    aiColor4D (0.3f, 0.4f ,0.0f, 1.0f), //dxf color code 66
    aiColor4D (0.4f, 0.4f ,0.3f, 1.0f), //dxf color code 67
    aiColor4D (0.2f, 0.3f ,0.0f, 1.0f), //dxf color code 68
    aiColor4D (0.3f, 0.3f ,0.2f, 1.0f), //dxf color code 69
    aiColor4D (0.5f, 1.0f ,0.0f, 1.0f), //dxf color code 70
    aiColor4D (0.8f, 1.0f ,0.7f, 1.0f), //dxf color code 71
    aiColor4D (0.4f, 0.7f ,0.0f, 1.0f), //dxf color code 72
    aiColor4D (0.6f, 0.7f ,0.5f, 1.0f), //dxf color code 73
    aiColor4D (0.3f, 0.5f ,0.0f, 1.0f), //dxf color code 74
    aiColor4D (0.4f, 0.5f ,0.3f, 1.0f), //dxf color code 75
    aiColor4D (0.2f, 0.4f ,0.0f, 1.0f), //dxf color code 76
    aiColor4D (0.3f, 0.4f ,0.3f, 1.0f), //dxf color code 77
    aiColor4D (0.2f, 0.3f ,0.0f, 1.0f), //dxf color code 78
    aiColor4D (0.3f, 0.3f ,0.2f, 1.0f), //dxf color code 79
    aiColor4D (0.2f, 1.0f ,0.0f, 1.0f), //dxf color code 80
    aiColor4D (0.7f, 1.0f ,0.7f, 1.0f), //dxf color code 81
    aiColor4D (0.2f, 0.7f ,0.0f, 1.0f), //dxf color code 82
    aiColor4D (0.6f, 0.7f ,0.5f, 1.0f), //dxf color code 83
    aiColor4D (0.1f, 0.5f ,0.0f, 1.0f), //dxf color code 84
    aiColor4D (0.4f, 0.5f ,0.3f, 1.0f), //dxf color code 85
    aiColor4D (0.1f, 0.4f ,0.0f, 1.0f), //dxf color code 86
    aiColor4D (0.3f, 0.4f ,0.3f, 1.0f), //dxf color code 87
    aiColor4D (0.1f, 0.3f ,0.0f, 1.0f), //dxf color code 88
    aiColor4D (0.2f, 0.3f ,0.2f, 1.0f), //dxf color code 89
    aiColor4D (0.0f, 1.0f ,0.0f, 1.0f), //dxf color code 90
    aiColor4D (0.7f, 1.0f ,0.7f, 1.0f), //dxf color code 91
    aiColor4D (0.0f, 0.7f ,0.0f, 1.0f), //dxf color code 92
    aiColor4D (0.5f, 0.7f ,0.5f, 1.0f), //dxf color code 93
    aiColor4D (0.0f, 0.5f ,0.0f, 1.0f), //dxf color code 94
    aiColor4D (0.3f, 0.5f ,0.3f, 1.0f), //dxf color code 95
    aiColor4D (0.0f, 0.4f ,0.0f, 1.0f), //dxf color code 96
    aiColor4D (0.3f, 0.4f ,0.3f, 1.0f), //dxf color code 97
    aiColor4D (0.0f, 0.3f ,0.0f, 1.0f), //dxf color code 98
    aiColor4D (0.2f, 0.3f ,0.2f, 1.0f), //dxf color code 99
    aiColor4D (0.0f, 1.0f ,0.2f, 1.0f), //dxf color code 100
    aiColor4D (0.7f, 1.0f ,0.7f, 1.0f), //dxf color code 101
    aiColor4D (0.0f, 0.7f ,0.2f, 1.0f), //dxf color code 102
    aiColor4D (0.5f, 0.7f ,0.6f, 1.0f), //dxf color code 103
    aiColor4D (0.0f, 0.5f ,0.1f, 1.0f), //dxf color code 104
    aiColor4D (0.3f, 0.5f ,0.4f, 1.0f), //dxf color code 105
    aiColor4D (0.0f, 0.4f ,0.1f, 1.0f), //dxf color code 106
    aiColor4D (0.3f, 0.4f ,0.3f, 1.0f), //dxf color code 107
    aiColor4D (0.0f, 0.3f ,0.1f, 1.0f), //dxf color code 108
    aiColor4D (0.2f, 0.3f ,0.2f, 1.0f), //dxf color code 109
    aiColor4D (0.0f, 1.0f ,0.5f, 1.0f), //dxf color code 110
    aiColor4D (0.7f, 1.0f ,0.8f, 1.0f), //dxf color code 111
    aiColor4D (0.0f, 0.7f ,0.4f, 1.0f), //dxf color code 112
    aiColor4D (0.5f, 0.7f ,0.6f, 1.0f), //dxf color code 113
    aiColor4D (0.0f, 0.5f ,0.3f, 1.0f), //dxf color code 114
    aiColor4D (0.3f, 0.5f ,0.4f, 1.0f), //dxf color code 115
    aiColor4D (0.0f, 0.4f ,0.2f, 1.0f), //dxf color code 116
    aiColor4D (0.3f, 0.4f ,0.3f, 1.0f), //dxf color code 117
    aiColor4D (0.0f, 0.3f ,0.2f, 1.0f), //dxf color code 118
    aiColor4D (0.2f, 0.3f ,0.3f, 1.0f), //dxf color code 119
    aiColor4D (0.0f, 1.0f ,0.7f, 1.0f), //dxf color code 120
    aiColor4D (0.7f, 1.0f ,0.9f, 1.0f), //dxf color code 121
    aiColor4D (0.0f, 0.7f ,0.6f, 1.0f), //dxf color code 122
    aiColor4D (0.5f, 0.7f ,0.7f, 1.0f), //dxf color code 123
    aiColor4D (0.0f, 0.5f ,0.4f, 1.0f), //dxf color code 124
    aiColor4D (0.3f, 0.5f ,0.5f, 1.0f), //dxf color code 125
    aiColor4D (0.0f, 0.4f ,0.3f, 1.0f), //dxf color code 126
    aiColor4D (0.3f, 0.4f ,0.4f, 1.0f), //dxf color code 127
    aiColor4D (0.0f, 0.3f ,0.2f, 1.0f), //dxf color code 128
    aiColor4D (0.2f, 0.3f ,0.3f, 1.0f), //dxf color code 129
    aiColor4D (0.0f, 1.0f ,1.0f, 1.0f), //dxf color code 130
    aiColor4D (0.7f, 1.0f ,1.0f, 1.0f), //dxf color code 131
    aiColor4D (0.0f, 0.7f ,0.7f, 1.0f), //dxf color code 132
    aiColor4D (0.5f, 0.7f ,0.7f, 1.0f), //dxf color code 133
    aiColor4D (0.0f, 0.5f ,0.5f, 1.0f), //dxf color code 134
    aiColor4D (0.3f, 0.5f ,0.5f, 1.0f), //dxf color code 135
    aiColor4D (0.0f, 0.4f ,0.4f, 1.0f), //dxf color code 136
    aiColor4D (0.3f, 0.4f ,0.4f, 1.0f), //dxf color code 137
    aiColor4D (0.0f, 0.3f ,0.3f, 1.0f), //dxf color code 138
    aiColor4D (0.2f, 0.3f ,0.3f, 1.0f), //dxf color code 139
    aiColor4D (0.0f, 0.7f ,1.0f, 1.0f), //dxf color code 140
    aiColor4D (0.7f, 0.9f ,1.0f, 1.0f), //dxf color code 141
    aiColor4D (0.0f, 0.6f ,0.7f, 1.0f), //dxf color code 142
    aiColor4D (0.5f, 0.7f ,0.7f, 1.0f), //dxf color code 143
    aiColor4D (0.0f, 0.4f ,0.5f, 1.0f), //dxf color code 144
    aiColor4D (0.3f, 0.5f ,0.5f, 1.0f), //dxf color code 145
    aiColor4D (0.0f, 0.3f ,0.4f, 1.0f), //dxf color code 146
    aiColor4D (0.3f, 0.4f ,0.4f, 1.0f), //dxf color code 147
    aiColor4D (0.0f, 0.2f ,0.3f, 1.0f), //dxf color code 148
    aiColor4D (0.2f, 0.3f ,0.3f, 1.0f), //dxf color code 149
    aiColor4D (0.0f, 0.5f ,1.0f, 1.0f), //dxf color code 150
    aiColor4D (0.7f, 0.8f ,1.0f, 1.0f), //dxf color code 151
    aiColor4D (0.0f, 0.4f ,0.7f, 1.0f), //dxf color code 152
    aiColor4D (0.5f, 0.6f ,0.7f, 1.0f), //dxf color code 153
    aiColor4D (0.0f, 0.3f ,0.5f, 1.0f), //dxf color code 154
    aiColor4D (0.3f, 0.4f ,0.5f, 1.0f), //dxf color code 155
    aiColor4D (0.0f, 0.2f ,0.4f, 1.0f), //dxf color code 156
    aiColor4D (0.3f, 0.3f ,0.4f, 1.0f), //dxf color code 157
    aiColor4D (0.0f, 0.2f ,0.3f, 1.0f), //dxf color code 158
    aiColor4D (0.2f, 0.3f ,0.3f, 1.0f), //dxf color code 159
    aiColor4D (0.0f, 0.2f ,1.0f, 1.0f), //dxf color code 160
    aiColor4D (0.7f, 0.7f ,1.0f, 1.0f), //dxf color code 161
    aiColor4D (0.0f, 0.2f ,0.7f, 1.0f), //dxf color code 162
    aiColor4D (0.5f, 0.6f ,0.7f, 1.0f), //dxf color code 163
    aiColor4D (0.0f, 0.1f ,0.5f, 1.0f), //dxf color code 164
    aiColor4D (0.3f, 0.4f ,0.5f, 1.0f), //dxf color code 165
    aiColor4D (0.0f, 0.1f ,0.4f, 1.0f), //dxf color code 166
    aiColor4D (0.3f, 0.3f ,0.4f, 1.0f), //dxf color code 167
    aiColor4D (0.0f, 0.1f ,0.3f, 1.0f), //dxf color code 168
    aiColor4D (0.2f, 0.2f ,0.3f, 1.0f), //dxf color code 169
    aiColor4D (0.0f, 0.0f ,1.0f, 1.0f), //dxf color code 170
    aiColor4D (0.7f, 0.7f ,1.0f, 1.0f), //dxf color code 171
    aiColor4D (0.0f, 0.0f ,0.7f, 1.0f), //dxf color code 172
    aiColor4D (0.5f, 0.5f ,0.7f, 1.0f), //dxf color code 173
    aiColor4D (0.0f, 0.0f ,0.5f, 1.0f), //dxf color code 174
    aiColor4D (0.3f, 0.3f ,0.5f, 1.0f), //dxf color code 175
    aiColor4D (0.0f, 0.0f ,0.4f, 1.0f), //dxf color code 176
    aiColor4D (0.3f, 0.3f ,0.4f, 1.0f), //dxf color code 177
    aiColor4D (0.0f, 0.0f ,0.3f, 1.0f), //dxf color code 178
    aiColor4D (0.2f, 0.2f ,0.3f, 1.0f), //dxf color code 179
    aiColor4D (0.2f, 0.0f ,1.0f, 1.0f), //dxf color code 180
    aiColor4D (0.7f, 0.7f ,1.0f, 1.0f), //dxf color code 181
    aiColor4D (0.2f, 0.0f ,0.7f, 1.0f), //dxf color code 182
    aiColor4D (0.6f, 0.5f ,0.7f, 1.0f), //dxf color code 183
    aiColor4D (0.1f, 0.0f ,0.5f, 1.0f), //dxf color code 184
    aiColor4D (0.4f, 0.3f ,0.5f, 1.0f), //dxf color code 185
    aiColor4D (0.1f, 0.0f ,0.4f, 1.0f), //dxf color code 186
    aiColor4D (0.3f, 0.3f ,0.4f, 1.0f), //dxf color code 187
    aiColor4D (0.1f, 0.0f ,0.3f, 1.0f), //dxf color code 188
    aiColor4D (0.2f, 0.2f ,0.3f, 1.0f), //dxf color code 189
    aiColor4D (0.5f, 0.0f ,1.0f, 1.0f), //dxf color code 190
    aiColor4D (0.8f, 0.7f ,1.0f, 1.0f), //dxf color code 191
    aiColor4D (0.4f, 0.0f ,0.7f, 1.0f), //dxf color code 192
    aiColor4D (0.6f, 0.5f ,0.7f, 1.0f), //dxf color code 193
    aiColor4D (0.3f, 0.0f ,0.5f, 1.0f), //dxf color code 194
    aiColor4D (0.4f, 0.3f ,0.5f, 1.0f), //dxf color code 195
    aiColor4D (0.2f, 0.0f ,0.4f, 1.0f), //dxf color code 196
    aiColor4D (0.3f, 0.3f ,0.4f, 1.0f), //dxf color code 197
    aiColor4D (0.2f, 0.0f ,0.3f, 1.0f), //dxf color code 198
    aiColor4D (0.3f, 0.2f ,0.3f, 1.0f), //dxf color code 199
    aiColor4D (0.7f, 0.0f ,1.0f, 1.0f), //dxf color code 200
    aiColor4D (0.9f, 0.7f ,1.0f, 1.0f), //dxf color code 201
    aiColor4D (0.6f, 0.0f ,0.7f, 1.0f), //dxf color code 202
    aiColor4D (0.7f, 0.5f ,0.7f, 1.0f), //dxf color code 203
    aiColor4D (0.4f, 0.0f ,0.5f, 1.0f), //dxf color code 204
    aiColor4D (0.5f, 0.3f ,0.5f, 1.0f), //dxf color code 205
    aiColor4D (0.3f, 0.0f ,0.4f, 1.0f), //dxf color code 206
    aiColor4D (0.4f, 0.3f ,0.4f, 1.0f), //dxf color code 207
    aiColor4D (0.2f, 0.0f ,0.3f, 1.0f), //dxf color code 208
    aiColor4D (0.3f, 0.2f ,0.3f, 1.0f), //dxf color code 209
    aiColor4D (1.0f, 0.0f ,1.0f, 1.0f), //dxf color code 210
    aiColor4D (1.0f, 0.7f ,1.0f, 1.0f), //dxf color code 211
    aiColor4D (0.7f, 0.0f ,0.7f, 1.0f), //dxf color code 212
    aiColor4D (0.7f, 0.5f ,0.7f, 1.0f), //dxf color code 213
    aiColor4D (0.5f, 0.0f ,0.5f, 1.0f), //dxf color code 214
    aiColor4D (0.5f, 0.3f ,0.5f, 1.0f), //dxf color code 215
    aiColor4D (0.4f, 0.0f ,0.4f, 1.0f), //dxf color code 216
    aiColor4D (0.4f, 0.3f ,0.4f, 1.0f), //dxf color code 217
    aiColor4D (0.3f, 0.0f ,0.3f, 1.0f), //dxf color code 218
    aiColor4D (0.3f, 0.2f ,0.3f, 1.0f), //dxf color code 219
    aiColor4D (1.0f, 0.0f ,0.7f, 1.0f), //dxf color code 220
    aiColor4D (1.0f, 0.7f ,0.9f, 1.0f), //dxf color code 221
    aiColor4D (0.7f, 0.0f ,0.6f, 1.0f), //dxf color code 222
    aiColor4D (0.7f, 0.5f ,0.7f, 1.0f), //dxf color code 223
    aiColor4D (0.5f, 0.0f ,0.4f, 1.0f), //dxf color code 224
    aiColor4D (0.5f, 0.3f ,0.5f, 1.0f), //dxf color code 225
    aiColor4D (0.4f, 0.0f ,0.3f, 1.0f), //dxf color code 226
    aiColor4D (0.4f, 0.3f ,0.4f, 1.0f), //dxf color code 227
    aiColor4D (0.3f, 0.0f ,0.2f, 1.0f), //dxf color code 228
    aiColor4D (0.3f, 0.2f ,0.3f, 1.0f), //dxf color code 229
    aiColor4D (1.0f, 0.0f ,0.5f, 1.0f), //dxf color code 230
    aiColor4D (1.0f, 0.7f ,0.8f, 1.0f), //dxf color code 231
    aiColor4D (0.7f, 0.0f ,0.4f, 1.0f), //dxf color code 232
    aiColor4D (0.7f, 0.5f ,0.6f, 1.0f), //dxf color code 233
    aiColor4D (0.5f, 0.0f ,0.3f, 1.0f), //dxf color code 234
    aiColor4D (0.5f, 0.3f ,0.4f, 1.0f), //dxf color code 235
    aiColor4D (0.4f, 0.0f ,0.2f, 1.0f), //dxf color code 236
    aiColor4D (0.4f, 0.3f ,0.3f, 1.0f), //dxf color code 237
    aiColor4D (0.3f, 0.0f ,0.2f, 1.0f), //dxf color code 238
    aiColor4D (0.3f, 0.2f ,0.3f, 1.0f), //dxf color code 239
    aiColor4D (1.0f, 0.0f ,0.2f, 1.0f), //dxf color code 240
    aiColor4D (1.0f, 0.7f ,0.7f, 1.0f), //dxf color code 241
    aiColor4D (0.7f, 0.0f ,0.2f, 1.0f), //dxf color code 242
    aiColor4D (0.7f, 0.5f ,0.6f, 1.0f), //dxf color code 243
    aiColor4D (0.5f, 0.0f ,0.1f, 1.0f), //dxf color code 244
    aiColor4D (0.5f, 0.3f ,0.4f, 1.0f), //dxf color code 245
    aiColor4D (0.4f, 0.0f ,0.1f, 1.0f), //dxf color code 246
    aiColor4D (0.4f, 0.3f ,0.3f, 1.0f), //dxf color code 247
    aiColor4D (0.3f, 0.0f ,0.1f, 1.0f), //dxf color code 248
    aiColor4D (0.3f, 0.2f ,0.2f, 1.0f), //dxf color code 249
    aiColor4D (0.2f, 0.2f ,0.2f, 1.0f), //dxf color code 250
    aiColor4D (0.3f, 0.3f ,0.3f, 1.0f), //dxf color code 251
    aiColor4D (0.4f, 0.4f ,0.4f, 1.0f), //dxf color code 252
    aiColor4D (0.5f, 0.5f ,0.5f, 1.0f), //dxf color code 253
    aiColor4D (0.7f, 0.7f ,0.7f, 1.0f), //dxf color code 254
    aiColor4D (1.0f, 1.0f ,1.0f, 1.0f) //dxf color code 255
};

#define AI_DXF_NUM_INDEX_COLORS (sizeof(g_aclrDxfIndexColors)/sizeof(g_aclrDxfIndexColors[0]))
#define AI_DXF_ENTITIES_MAGIC_BLOCK "$ASSIMP_ENTITIES_MAGIC"

static const int GroupCode_Name  = 2;
static const int GroupCode_XComp = 10;
static const int GroupCode_YComp = 20;
static const int GroupCode_ZComp = 30;

static constexpr aiImporterDesc desc = {
    "Drawing Interchange Format (DXF) Importer",
    "",
    "",
    "",
    aiImporterFlags_SupportTextFlavour | aiImporterFlags_LimitedSupport,
    0,
    0,
    0,
    0,
    "dxf"
};

// ------------------------------------------------------------------------------------------------
// Returns whether the class can handle the format of the given file.
bool DXFImporter::CanRead( const std::string& filename, IOSystem* pIOHandler, bool /*checkSig*/ ) const {
    static const char *tokens[] = { "SECTION", "HEADER", "ENDSEC", "BLOCKS" };
    return SearchFileHeaderForToken(pIOHandler, filename, tokens, AI_COUNT_OF(tokens), 32);
}

// ------------------------------------------------------------------------------------------------
// Get a list of all supported file extensions
const aiImporterDesc* DXFImporter::GetInfo () const {
    return &desc;
}

// ------------------------------------------------------------------------------------------------
// Imports the given file into the given scene structure.
void DXFImporter::InternReadFile( const std::string& filename, aiScene* pScene, IOSystem* pIOHandler) {
    std::shared_ptr<IOStream> file = std::shared_ptr<IOStream>( pIOHandler->Open( filename) );

    // Check whether we can read the file
    if (file == nullptr) {
        throw DeadlyImportError( "Failed to open DXF file ", filename, "");
    }

    // Check whether this is a binary DXF file - we can't read binary DXF files :-(
    char buff[AI_DXF_BINARY_IDENT_LEN] = {0};
    file->Read(buff,AI_DXF_BINARY_IDENT_LEN,1);

    if (0 == memcmp(AI_DXF_BINARY_IDENT,buff,AI_DXF_BINARY_IDENT_LEN)) {
        throw DeadlyImportError("DXF: Binary files are not supported at the moment");
    }

    // DXF files can grow very large, so read them via the StreamReader,
    // which will choose a suitable strategy.
    file->Seek(0,aiOrigin_SET);
    StreamReaderLE stream( std::move(file) );

    DXF::LineReader reader (stream);
    DXF::FileData output;

    // now get all lines of the file and process top-level sections
    bool eof = false;
    while(!reader.End()) {

        // blocks table - these 'build blocks' are later (in ENTITIES)
        // referenced an included via INSERT statements.
        if (reader.Is(2,"BLOCKS")) {
            ParseBlocks(reader,output);
            continue;
        }

        // primary entity table
        if (reader.Is(2,"ENTITIES")) {
            ParseEntities(reader,output);
            continue;
        }

        // skip unneeded sections entirely to avoid any problems with them
        // altogether.
        else if (reader.Is(2,"CLASSES") || reader.Is(2,"TABLES")) {
            SkipSection(reader);
            continue;
        }

        else if (reader.Is(2,"HEADER")) {
            ParseHeader(reader,output);
            continue;
        }

        // comments
        else if (reader.Is(999)) {
            ASSIMP_LOG_INFO("DXF Comment: ", reader.Value());
        }

        // don't read past the official EOF sign
        else if (reader.Is(0,"EOF")) {
            eof = true;
            break;
        }

        ++reader;
    }
    if (!eof) {
        ASSIMP_LOG_WARN("DXF: EOF reached, but did not encounter DXF EOF marker");
    }

    ConvertMeshes(pScene,output);

    // Now rotate the whole scene by 90 degrees around the x axis to convert from AutoCAD's to Assimp's coordinate system
    pScene->mRootNode->mTransformation = aiMatrix4x4(
        1.f,0.f,0.f,0.f,
        0.f,0.f,1.f,0.f,
        0.f,-1.f,0.f,0.f,
        0.f,0.f,0.f,1.f) * pScene->mRootNode->mTransformation;
}

// ------------------------------------------------------------------------------------------------
void DXFImporter::ConvertMeshes(aiScene* pScene, DXF::FileData& output) {
    // the process of resolving all the INSERT statements can grow the
    // poly-count excessively, so log the original number.
    // XXX Option to import blocks as separate nodes?
    if (!DefaultLogger::isNullLogger()) {
        unsigned int vcount = 0, icount = 0;
        for (const DXF::Block& bl : output.blocks) {
            for (std::shared_ptr<const DXF::PolyLine> pl : bl.lines) {
                vcount += static_cast<unsigned int>(pl->positions.size());
                icount += static_cast<unsigned int>(pl->counts.size());
            }
        }

        ASSIMP_LOG_VERBOSE_DEBUG("DXF: Unexpanded polycount is ", icount, ", vertex count is ", vcount);
    }

    if (output.blocks.empty()) {
        throw DeadlyImportError("DXF: no data blocks loaded");
    }

    DXF::Block* entities( nullptr );

    // index blocks by name
    DXF::BlockMap blocks_by_name;
    for (DXF::Block& bl : output.blocks) {
        blocks_by_name[bl.name] = &bl;
        if ( !entities && bl.name == AI_DXF_ENTITIES_MAGIC_BLOCK ) {
            entities = &bl;
        }
    }

    if (!entities) {
        throw DeadlyImportError("DXF: no ENTITIES data block loaded");
    }

    typedef std::map<std::string, unsigned int> LayerMap;

    LayerMap layers;
    std::vector< std::vector< const DXF::PolyLine*> > corr;

    // now expand all block references in the primary ENTITIES block
    // XXX this involves heavy memory copying, consider a faster solution for future versions.
    ExpandBlockReferences(*entities,blocks_by_name);

    unsigned int cur = 0;
    for (std::shared_ptr<const DXF::PolyLine> pl : entities->lines) {
        if (pl->positions.size()) {

            std::map<std::string, unsigned int>::iterator it = layers.find(pl->layer);
            if (it == layers.end()) {
                ++pScene->mNumMeshes;

                layers[pl->layer] = cur++;

                std::vector< const DXF::PolyLine* > pv;
                pv.push_back(&*pl);

                corr.push_back(pv);
            }
            else {
                corr[(*it).second].push_back(&*pl);
            }
        }
    }

    if ( 0 == pScene->mNumMeshes) {
        throw DeadlyImportError("DXF: this file contains no 3d data");
    }

    pScene->mMeshes = new aiMesh*[ pScene->mNumMeshes ] ();

    for(const LayerMap::value_type& elem : layers){
        aiMesh* const mesh =  pScene->mMeshes[elem.second] = new aiMesh();
        mesh->mName.Set(elem.first);

        unsigned int cvert = 0,cface = 0;
        for(const DXF::PolyLine* pl : corr[elem.second]){
            // sum over all faces since we need to 'verbosify' them.
            cvert += std::accumulate(pl->counts.begin(),pl->counts.end(),0);
            cface += static_cast<unsigned int>(pl->counts.size());
        }

        aiVector3D* verts = mesh->mVertices = new aiVector3D[cvert];
        aiColor4D* colors = mesh->mColors[0] = new aiColor4D[cvert];
        aiFace* faces = mesh->mFaces = new aiFace[cface];

        mesh->mNumVertices = cvert;
        mesh->mNumFaces = cface;

        unsigned int prims = 0;
        unsigned int overall_indices = 0;
        for(const DXF::PolyLine* pl : corr[elem.second]){

            std::vector<unsigned int>::const_iterator it = pl->indices.begin();
            for(unsigned int facenumv : pl->counts) {
                aiFace& face = *faces++;
                face.mIndices = new unsigned int[face.mNumIndices = facenumv];

                for (unsigned int i = 0; i < facenumv; ++i) {
                    face.mIndices[i] = overall_indices++;

                    ai_assert(pl->positions.size() == pl->colors.size());
                    if (*it >= pl->positions.size()) {
                        throw DeadlyImportError("DXF: vertex index out of bounds");
                    }

                    *verts++ = pl->positions[*it];
                    *colors++ = pl->colors[*it++];
                }

                // set primitive flags now, this saves the extra pass in ScenePreprocessor.
                switch(face.mNumIndices) {
                    case 1:
                        prims |= aiPrimitiveType_POINT;
                        break;
                    case 2:
                        prims |= aiPrimitiveType_LINE;
                        break;
                    case 3:
                        prims |= aiPrimitiveType_TRIANGLE;
                        break;
                    default:
                        prims |= aiPrimitiveType_POLYGON;
                        break;
                }
            }
        }

        mesh->mPrimitiveTypes = prims;
        mesh->mMaterialIndex = 0;
    }

    GenerateHierarchy(pScene,output);
    GenerateMaterials(pScene,output);
}


// ------------------------------------------------------------------------------------------------
void DXFImporter::ExpandBlockReferences(DXF::Block& bl,const DXF::BlockMap& blocks_by_name) {
    for (const DXF::InsertBlock& insert : bl.insertions) {

        // first check if the referenced blocks exists ...
        const DXF::BlockMap::const_iterator it = blocks_by_name.find(insert.name);
        if (it == blocks_by_name.end()) {
            ASSIMP_LOG_ERROR("DXF: Failed to resolve block reference: ", insert.name,"; skipping" );
            continue;
        }

        // XXX this would be the place to implement recursive expansion if needed.
        const DXF::Block& bl_src = *(*it).second;

        const size_t size = bl_src.lines.size(); // the size may increase in the loop
        for (size_t i = 0; i < size; ++i) {
            std::shared_ptr<const DXF::PolyLine> pl_in = bl_src.lines[i];
            if (!pl_in) {
                ASSIMP_LOG_ERROR("DXF: PolyLine instance is nullptr, skipping.");
                continue;
            }

            std::shared_ptr<DXF::PolyLine> pl_out = std::shared_ptr<DXF::PolyLine>(new DXF::PolyLine(*pl_in));

            if (bl_src.base.Length() || insert.scale.x!=1.f || insert.scale.y!=1.f || insert.scale.z!=1.f || insert.angle || insert.pos.Length()) {
                // manual coordinate system transformation
                // XXX order
                aiMatrix4x4 trafo, tmp;
                aiMatrix4x4::Translation(-bl_src.base,trafo);
                //Need to translate position before scaling the insert
                //otherwise the position ends up being the position*scaling
                //STH 2024.01.17
                trafo *= aiMatrix4x4::Translation(insert.pos,tmp);
                trafo *= aiMatrix4x4::Scaling(insert.scale,tmp);
                //trafo *= aiMatrix4x4::Translation(insert.pos,tmp);

                // XXX rotation currently ignored - I didn't find an appropriate sample model.
                if (insert.angle != 0.f) {
                    ASSIMP_LOG_WARN("DXF: BLOCK rotation not currently implemented");
                }

                for (aiVector3D& v : pl_out->positions) {
                    v *= trafo;
                }
            }

            bl.lines.push_back(pl_out);
        }
    }
}

// ------------------------------------------------------------------------------------------------
void DXFImporter::GenerateMaterials(aiScene* pScene, DXF::FileData& /*output*/) {
    // generate an almost-white default material. Reason:
    // the default vertex color is GREY, so we are
    // already at Assimp's usual default color.
    // generate a default material
    aiMaterial* pcMat = new aiMaterial();
    aiString s;
    s.Set(AI_DEFAULT_MATERIAL_NAME);
    pcMat->AddProperty(&s, AI_MATKEY_NAME);

    aiColor4D clrDiffuse(0.9f,0.9f,0.9f,1.0f);
    pcMat->AddProperty(&clrDiffuse,1,AI_MATKEY_COLOR_DIFFUSE);

    clrDiffuse = aiColor4D(1.0f,1.0f,1.0f,1.0f);
    pcMat->AddProperty(&clrDiffuse,1,AI_MATKEY_COLOR_SPECULAR);

    clrDiffuse = aiColor4D(0.05f,0.05f,0.05f,1.0f);
    pcMat->AddProperty(&clrDiffuse,1,AI_MATKEY_COLOR_AMBIENT);

    pScene->mNumMaterials = 1;
    pScene->mMaterials = new aiMaterial*[1];
    pScene->mMaterials[0] = pcMat;
}

// ------------------------------------------------------------------------------------------------
void DXFImporter::GenerateHierarchy(aiScene* pScene, DXF::FileData& /*output*/) {
    // generate the output scene graph, which is just the root node with a single child for each layer.
    pScene->mRootNode = new aiNode();
    pScene->mRootNode->mName.Set("<DXF_ROOT>");

    if (1 == pScene->mNumMeshes)    {
        pScene->mRootNode->mMeshes = new unsigned int[ pScene->mRootNode->mNumMeshes = 1 ];
        pScene->mRootNode->mMeshes[0] = 0;
    } else {
        pScene->mRootNode->mChildren = new aiNode*[ pScene->mRootNode->mNumChildren = pScene->mNumMeshes ];
        for (unsigned int m = 0; m < pScene->mRootNode->mNumChildren;++m)   {
            aiNode* p = pScene->mRootNode->mChildren[m] = new aiNode();
            p->mName = pScene->mMeshes[m]->mName;

            p->mMeshes = new unsigned int[p->mNumMeshes = 1];
            p->mMeshes[0] = m;
            p->mParent = pScene->mRootNode;
        }
    }
}


// ------------------------------------------------------------------------------------------------
void DXFImporter::SkipSection(DXF::LineReader& reader) {
    for( ;!reader.End() && !reader.Is(0,"ENDSEC"); reader++);
}

// ------------------------------------------------------------------------------------------------
void DXFImporter::ParseHeader(DXF::LineReader& reader, DXF::FileData& ) {
    for( ;!reader.End() && !reader.Is(0,"ENDSEC"); reader++);
}

// ------------------------------------------------------------------------------------------------
void DXFImporter::ParseBlocks(DXF::LineReader& reader, DXF::FileData& output) {
    while( !reader.End() && !reader.Is(0,"ENDSEC")) {
        if (reader.Is(0,"BLOCK")) {
            ParseBlock(++reader,output);
            continue;
        }
        ++reader;
    }

    ASSIMP_LOG_VERBOSE_DEBUG("DXF: got ", output.blocks.size()," entries in BLOCKS" );
}

// ------------------------------------------------------------------------------------------------
void DXFImporter::ParseBlock(DXF::LineReader& reader, DXF::FileData& output) {
    // push a new block onto the stack.
    output.blocks.emplace_back();
    DXF::Block& block = output.blocks.back();

    while( !reader.End() && !reader.Is(0,"ENDBLK")) {

        switch(reader.GroupCode()) {
            case GroupCode_Name:
                block.name = reader.Value();
                break;

            case GroupCode_XComp:
                block.base.x = reader.ValueAsFloat();
                break;
            case GroupCode_YComp:
                block.base.y = reader.ValueAsFloat();
                break;
            case GroupCode_ZComp:
                block.base.z = reader.ValueAsFloat();
                break;
        }

        if (reader.Is(0,"POLYLINE")) {
            ParsePolyLine(++reader,output);
            continue;
        }
        else if (reader.Is(0,"LWPOLYLINE")) {
            ParseLWPolyLine(++reader,output);
            continue;
        }
        // valid case
        else if (reader.Is(0,"INSERT")) {
            ASSIMP_LOG_WARN("DXF: INSERT within a BLOCK not currently supported; skipping");
            for( ;!reader.End() && (reader.Is(0,"ATTRIB") || reader.Is(0,"SEQEND")); ++reader);
            break;
        }
        else if (reader.Is(0,"3DFACE") || reader.Is(0,"SOLID")
                 || reader.Is(0,"LINE") || reader.Is(0,"3DLINE")) {
            //http://sourceforge.net/tracker/index.php?func=detail&aid=2970566&group_id=226462&atid=1067632
            Parse3DFace(++reader, output);
            continue;
        }
        else if (reader.Is(0,"3DSOLID") || reader.Is(0,"REGION") || reader.Is(0,"BODY")) {
            Parse3DSolid(++reader, output);
            continue;
        }
        else if (reader.Is(0,"MESH")) {
            ParseMesh(++reader, output);
            continue;
        }
        else if (reader.Is(0,"SURFACE")
                 || reader.Is(0,"EXTRUDEDSURFACE")
                 || reader.Is(0,"LOFTEDSURFACE")
                 || reader.Is(0,"NURBSURFACE")
                 || reader.Is(0,"PLANESURFACE")
                 || reader.Is(0,"REVOLVEDSURFACE")
                 || reader.Is(0,"SWEPTSURFACE")) {
            ParseSurface(++reader, output);
            continue;
        }
        else if (reader.Is(0,"ACSH_BOOLEAN_CLASS")
                 || reader.Is(0,"ACSH_BOX_CLASS") // same as Wedge
                 || reader.Is(0,"ACSH_BREP_CLASS")
                 || reader.Is(0,"ACSH_CHAMFER_CLASS")
                 || reader.Is(0,"ACSH_CONE_CLASS")
                 || reader.Is(0,"ACSH_CYLINDER_CLASS")
                 || reader.Is(0,"ACSH_EXTRUSION_CLASS")
                 || reader.Is(0,"ACSH_FILLET_CLASS")
                 || reader.Is(0,"ACSH_HISTORY_CLASS")
                 || reader.Is(0,"ACSH_LOFT_CLASS")
                 || reader.Is(0,"ACSH_PYRAMID_CLASS")
                 || reader.Is(0,"ACSH_REVOLVE_CLASS")
                 || reader.Is(0,"ACSH_SPHERE_CLASS")
                 || reader.Is(0,"ACSH_SWEEP_CLASS")
                 || reader.Is(0,"ACSH_TORUS_CLASS")
                 || reader.Is(0,"ACSH_WEDGE_CLASS")) {
            ParseAcShClass(++reader, output);
            continue;
        }
        ++reader;
    }
}

// ------------------------------------------------------------------------------------------------
void DXFImporter::ParseEntities(DXF::LineReader& reader, DXF::FileData& output) {
    // Push a new block onto the stack.
    output.blocks.emplace_back();
    DXF::Block& block = output.blocks.back();

    block.name = AI_DXF_ENTITIES_MAGIC_BLOCK;

    while( !reader.End() && !reader.Is(0,"ENDSEC")) {
        if (reader.Is(0,"POLYLINE")) {
            ParsePolyLine(++reader,output);
            continue;
        }
        else if (reader.Is(0,"LWPOLYLINE")) {
            ParseLWPolyLine(++reader,output);
            continue;
        }

        else if (reader.Is(0,"INSERT")) {
            ParseInsertion(++reader,output);
            continue;
        }

        else if (reader.Is(0,"3DFACE") || reader.Is(0,"SOLID")
                 || reader.Is(0,"LINE") || reader.Is(0,"3DLINE")) {
            //http://sourceforge.net/tracker/index.php?func=detail&aid=2970566&group_id=226462&atid=1067632
            Parse3DFace(++reader, output);
            continue;
        }
        else if (reader.Is(0,"3DSOLID") || reader.Is(0,"REGION") || reader.Is(0,"BODY")) {
            Parse3DSolid(++reader, output);
            continue;
        }
        else if (reader.Is(0,"MESH")) {
            ParseMesh(++reader, output);
            continue;
        }
        else if (reader.Is(0,"SURFACE")
                 || reader.Is(0,"EXTRUDEDSURFACE")
                 || reader.Is(0,"LOFTEDSURFACE")
                 || reader.Is(0,"NURBSURFACE")
                 || reader.Is(0,"PLANESURFACE")
                 || reader.Is(0,"REVOLVEDSURFACE")
                 || reader.Is(0,"SWEPTSURFACE")) {
            ParseSurface(++reader, output);
            continue;
        }
        else if (reader.Is(0,"ACSH_BOOLEAN_CLASS")
                 || reader.Is(0,"ACSH_BOX_CLASS")
                 || reader.Is(0,"ACSH_BREP_CLASS")
                 || reader.Is(0,"ACSH_CHAMFER_CLASS")
                 || reader.Is(0,"ACSH_CONE_CLASS")
                 || reader.Is(0,"ACSH_CYLINDER_CLASS")
                 || reader.Is(0,"ACSH_EXTRUSION_CLASS")
                 || reader.Is(0,"ACSH_FILLET_CLASS")
                 || reader.Is(0,"ACSH_HISTORY_CLASS")
                 || reader.Is(0,"ACSH_LOFT_CLASS")
                 || reader.Is(0,"ACSH_PYRAMID_CLASS")
                 || reader.Is(0,"ACSH_REVOLVE_CLASS")
                 || reader.Is(0,"ACSH_SPHERE_CLASS")
                 || reader.Is(0,"ACSH_SWEEP_CLASS")
                 || reader.Is(0,"ACSH_TORUS_CLASS")
                 || reader.Is(0,"ACSH_WEDGE_CLASS")) {
            ParseAcShClass(++reader, output);
            continue;
        }
        ++reader;
    }

    ASSIMP_LOG_VERBOSE_DEBUG( "DXF: got ", block.lines.size()," polylines and ", block.insertions.size(),
        " inserted blocks in ENTITIES" );
}

void DXFImporter::ParseInsertion(DXF::LineReader& reader, DXF::FileData& output) {
    output.blocks.back().insertions.emplace_back();
    DXF::InsertBlock& bl = output.blocks.back().insertions.back();

    while( !reader.End() && !reader.Is(0)) {
        switch(reader.GroupCode()) {
            // name of referenced block
        case GroupCode_Name:
            bl.name = reader.Value();
            break;

            // translation
        case GroupCode_XComp:
            bl.pos.x = reader.ValueAsFloat();
            break;
        case GroupCode_YComp:
            bl.pos.y = reader.ValueAsFloat();
            break;
        case GroupCode_ZComp:
            bl.pos.z = reader.ValueAsFloat();
            break;

            // scaling
        case 41:
            bl.scale.x = reader.ValueAsFloat();
            break;
        case 42:
            bl.scale.y = reader.ValueAsFloat();
            break;
        case 43:
            bl.scale.z = reader.ValueAsFloat();
            break;

            // rotation angle
        case 50:
            bl.angle = reader.ValueAsFloat();
            break;
        }
        reader++;
    }
}

static constexpr unsigned int DXF_POLYLINE_FLAG_CLOSED = 0x1;
// Currently unused
//static constexpr unsigned int DXF_POLYLINE_FLAG_CURVE_FIT = 0x2;
//static constexpr unsigned int DXF_POLYLINE_FLAG_SPLINE_FIT = 0x4;
//static constexpr unsigned int DXF_POLYLINE_FLAG_3D_POLYLINE = 0x8;
//static constexpr unsigned int DXF_POLYLINE_FLAG_3D_POLYMESH = 0x10;
//static constexpr unsigned int DXF_POLYLINE_FLAG_MESH_CLOSED_IN_N = 0x20;
//static constexpr unsigned int DXF_POLYLINE_FLAG_LTYPE_CONTINUOUS = 0x80;
static constexpr unsigned int DXF_POLYLINE_FLAG_POLYFACEMESH = 0x40;

// ------------------------------------------------------------------------------------------------
void DXFImporter::ParsePolyLine(DXF::LineReader& reader, DXF::FileData& output) {
    output.blocks.back().lines.push_back( std::shared_ptr<DXF::PolyLine>( new DXF::PolyLine() ) );
    DXF::PolyLine& line = *output.blocks.back().lines.back();

    unsigned int iguess = 0, vguess = 0;
    while( !reader.End() && !reader.Is(0,"ENDSEC")) {

        if (reader.Is(0,"VERTEX")) {
            ParsePolyLineVertex(++reader,line);
            if (reader.Is(0,"SEQEND")) {
                break;
            }
            continue;
        }

        switch(reader.GroupCode())
        {
        // flags --- important that we know whether it is a
        // polyface mesh or 'just' a line.
        case 70:
            if (!line.flags)    {
                line.flags = reader.ValueAsSignedInt();
            }
            break;

        // optional number of vertices
        case 71:
            vguess = reader.ValueAsSignedInt();
            line.positions.reserve(vguess);
            break;

        // optional number of faces
        case 72:
            iguess = reader.ValueAsSignedInt();
            line.indices.reserve(iguess);
            break;

        // 8 specifies the layer on which this line is placed on
        case 8:
            line.layer = reader.Value();
            break;
        }

        reader++;
    }

    if (vguess && line.positions.size() != vguess) {
        ASSIMP_LOG_WARN("DXF: unexpected vertex count in polymesh: ",
            line.positions.size(),", expected ", vguess );
    }

    if (line.flags & DXF_POLYLINE_FLAG_POLYFACEMESH ) {
        if (line.positions.size() < 3 || line.indices.size() < 3)   {
            ASSIMP_LOG_WARN("DXF: not enough vertices for polymesh; ignoring");
            output.blocks.back().lines.pop_back();
            return;
        }

        // if these numbers are wrong, parsing might have gone wild.
        // however, the docs state that applications are not required
        // to set the 71 and 72 fields, respectively, to valid values.
        // So just fire a warning.
        if (iguess && line.counts.size() != iguess) {
            ASSIMP_LOG_WARN( "DXF: unexpected face count in polymesh: ", line.counts.size(),", expected ", iguess );
        }
    }
    else if (!line.indices.size() && !line.counts.size()) {
        // a poly-line - so there are no indices yet.
        size_t guess = line.positions.size() + (line.flags & DXF_POLYLINE_FLAG_CLOSED ? 1 : 0);
        line.indices.reserve(guess);

        line.counts.reserve(guess/2);
        for (unsigned int i = 0; i < line.positions.size()/2; ++i) {
            line.indices.push_back(i*2);
            line.indices.push_back(i*2+1);
            line.counts.push_back(2);
        }

        // closed polyline?
        if (line.flags & DXF_POLYLINE_FLAG_CLOSED) {
            line.indices.push_back(static_cast<unsigned int>(line.positions.size()-1));
            line.indices.push_back(0);
            line.counts.push_back(2);
        }
    }
}

#define DXF_VERTEX_FLAG_PART_OF_POLYFACE 0x80
#define DXF_VERTEX_FLAG_HAS_POSITIONS 0x40

// ------------------------------------------------------------------------------------------------
void DXFImporter::ParsePolyLineVertex(DXF::LineReader& reader, DXF::PolyLine& line) {
    unsigned int cnti = 0, flags = 0;
    unsigned int indices[4];

    aiVector3D out;
    aiColor4D clr = AI_DXF_DEFAULT_COLOR;

    while( !reader.End() ) {

        if (reader.Is(0)) { // SEQEND or another VERTEX
            break;
        }

        switch (reader.GroupCode()) {
        case 8:
                // layer to which the vertex belongs to - assume that
                // this is always the layer the top-level poly-line
                // entity resides on as well.
                if(reader.Value() != line.layer) {
                    ASSIMP_LOG_WARN("DXF: expected vertex to be part of a poly-face but the 0x128 flag isn't set");
                }
                break;

        case 70:
                flags = reader.ValueAsUnsignedInt();
                break;

        // VERTEX COORDINATES
        case GroupCode_XComp:
            out.x = reader.ValueAsFloat();
            break;

        case GroupCode_YComp:
            out.y = reader.ValueAsFloat();
            break;

        case GroupCode_ZComp:
            out.z = reader.ValueAsFloat();
            break;

        // POLYFACE vertex indices
        case 71:
        case 72:
        case 73:
        case 74: {
                if (cnti == 4) {
                    ASSIMP_LOG_WARN("DXF: more than 4 indices per face not supported; ignoring");
                    break;
                }
                const int index = reader.ValueAsSignedInt();
                if (index >= 0) {
                    indices[cnti++] = static_cast<unsigned int>(index);
                } else {
                    indices[cnti++] = static_cast<unsigned int>(-index);
                }
            }
            indices[cnti++] = std::abs(reader.ValueAsSignedInt());
            break;

        // color
        case 62:
            clr = g_aclrDxfIndexColors[reader.ValueAsUnsignedInt() % AI_DXF_NUM_INDEX_COLORS];
            break;
        };

        reader++;
    }

    if (line.flags & DXF_POLYLINE_FLAG_POLYFACEMESH && !(flags & DXF_VERTEX_FLAG_PART_OF_POLYFACE)) {
        ASSIMP_LOG_WARN("DXF: expected vertex to be part of a polyface but the 0x128 flag isn't set");
    }

    if (cnti) {
        line.counts.push_back(cnti);
        for (unsigned int i = 0; i < cnti; ++i) {
            // IMPORTANT NOTE: POLYMESH indices are ONE-BASED
            if (indices[i] == 0) {
                ASSIMP_LOG_WARN("DXF: invalid vertex index, indices are one-based.");
                --line.counts.back();
                // Workaround to fix issue 2229
                if (line.counts.back() == 0) {
                    line.counts.pop_back();
                }
                continue;
            }
            line.indices.push_back(indices[i]-1);
        }
    } else {
        line.positions.push_back(out);
        line.colors.push_back(clr);
    }
}

// ------------------------------------------------------------------------------------------------
void DXFImporter::Parse3DFace(DXF::LineReader& reader, DXF::FileData& output) {
    // (note) this is also used for for parsing line entities, so we
    // must handle the vertex_count == 2 case as well.

    output.blocks.back().lines.push_back( std::shared_ptr<DXF::PolyLine>( new DXF::PolyLine() )  );
    DXF::PolyLine& line = *output.blocks.back().lines.back();

    aiVector3D vip[4];
    aiColor4D  clr = AI_DXF_DEFAULT_COLOR;

    bool b[4] = {false,false,false,false};
    while( !reader.End() ) {

        // next entity with a groupcode == 0 is probably already the next vertex or polymesh entity
        if (reader.GroupCode() == 0) {
            break;
        }
        switch (reader.GroupCode()) {

        // 8 specifies the layer
        case 8:
            line.layer = reader.Value();
            break;

        // x position of the first corner
        case 10:
            vip[0].x = reader.ValueAsFloat();
            b[2] = true;
            break;

        // y position of the first corner
        case 20:
            vip[0].y = reader.ValueAsFloat();
            b[2] = true;
            break;

        // z position of the first corner
        case 30:
            vip[0].z = reader.ValueAsFloat();
            b[2] = true;
            break;

        // x position of the second corner
        case 11:
            vip[1].x = reader.ValueAsFloat();
            b[3] = true;
            break;

        // y position of the second corner
        case 21:
            vip[1].y = reader.ValueAsFloat();
            b[3] = true;
            break;

        // z position of the second corner
        case 31:
            vip[1].z = reader.ValueAsFloat();
            b[3] = true;
            break;

        // x position of the third corner
        case 12:
            vip[2].x = reader.ValueAsFloat();
            b[0] = true;
            break;

        // y position of the third corner
        case 22:
            vip[2].y = reader.ValueAsFloat();
            b[0] = true;
            break;

        // z position of the third corner
        case 32:
            vip[2].z = reader.ValueAsFloat();
            b[0] = true;
            break;

        // x position of the fourth corner
        case 13:
            vip[3].x = reader.ValueAsFloat();
            b[1] = true;
            break;

        // y position of the fourth corner
        case 23:
            vip[3].y = reader.ValueAsFloat();
            b[1] = true;
            break;

        // z position of the fourth corner
        case 33:
            vip[3].z = reader.ValueAsFloat();
            b[1] = true;
            break;

        // color
        case 62:
            clr = g_aclrDxfIndexColors[reader.ValueAsUnsignedInt() % AI_DXF_NUM_INDEX_COLORS];
            break;
        };

        ++reader;
    }

    // the fourth corner may even be identical to the third,
    // in this case we treat it as if it didn't exist.
    //if (vip[3] == vip[2]) { // ru: nope, accept triangles
    //    b[1] = false;
    //}

    // sanity checks to see if we got something meaningful
    if ((b[1] && !b[0]) || !b[2] || !b[3]) {
        ASSIMP_LOG_WARN("DXF: unexpected vertex setup in 3DFACE/LINE/SOLID entity; ignoring");
        output.blocks.back().lines.pop_back();
        return;
    }

    const unsigned int cnt = (2+(b[0]?1:0)+(b[1]?1:0));
    line.counts.push_back(cnt);

    for (unsigned int i = 0; i < cnt; ++i) {
        line.indices.push_back(static_cast<unsigned int>(line.positions.size()));
        line.positions.push_back(vip[i]);
        line.colors.push_back(clr);
    }
}

void ParseLWPolyLine(DXF::LineReader& /*reader*/, DXF::FileData& /*output*/)
{
    ASSIMP_LOG_WARN("DXF: LWPolyLine not currently supported; ignoring");
}
void Parse3DSolid(DXF::LineReader& /*reader*/, DXF::FileData& /*output*/)
{
    ASSIMP_LOG_WARN("DXF: 3DSOLID not currently supported; ignoring");
}
void ParseMesh(DXF::LineReader& /*reader*/, DXF::FileData& /*output*/)
{
    ASSIMP_LOG_WARN("DXF: MESH not currently supported; ignoring");
}
void ParseSurface(DXF::LineReader& /*reader*/, DXF::FileData& /*output*/)
{
    ASSIMP_LOG_WARN("DXF: Surface not currently supported; ignoring");
}
void ParseAcShClass(DXF::LineReader& /*reader*/, DXF::FileData& /*output*/)
{
    ASSIMP_LOG_WARN("DXF: AcShClass not currently supported; ignoring");
}

#endif // !! ASSIMP_BUILD_NO_DXF_IMPORTER
