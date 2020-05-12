/* ULP Example: Read temperautre in deep sleep

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.

   This file contains assembly code which runs on the ULP.

*/

/* ULP assembly files are passed through C preprocessor first, so include directives
   and C macros may be used in these files 
 */

.macro push rx
  st \rx,r3,0
  sub r3,r3,1
.endm

.macro pop rx
  add r3,r3,1
  ld \rx,r3,0
.endm

// Subroutine jump
.macro call target
  .set addr,(.+16)
  move r0,addr
  push r0
  jump \target
.endm

// Return from subroutine
.macro ret 
  pop r0
  jump r0
.endm

// Assign value to variable
.macro assign variable, value
  move r1, \value
  move r0, \variable
  st r1, r0, 0
.endm

// Fetch variable directly into register rx
.macro fetch rx, variable
  move r0, \variable
  ld \rx, r0, 0
.endm

// Fetch variable[ry] directly into register rx,
.macro fetch_array rx, variable, ry
  move r0, \variable
  add r0, r0, \ry
  ld \rx, r0, 0
.endm

// Increment variable
.macro increment variable amount
  move r0, \variable
  ld r1, r0, 0
  add r1, r1, \amount
  st r1, r0, 0
.endm
