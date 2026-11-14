# MASK-GW-018: merge yes: a square annulus, a U-shape, a comb over 200 points and an isolated rectangle.
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
probe {drc off}
probe {tech load input/masktest -noprompt}
probe {snap internal}
probe {gds datestamp 0}
probe {load top}
probe {box 0 0 10 10}
probe {paint metal1}
probe {box 3 3 7 7}
probe {erase metal1}
probe {box 20 0 30 2}
probe {paint metal1}
probe {box 20 2 22 10}
probe {paint metal1}
probe {box 28 2 30 10}
probe {paint metal1}
probe {box 0 20 400 22}
probe {paint metal1}
probe {box 0 22 2 30}
probe {paint metal1}
probe {box 4 22 6 30}
probe {paint metal1}
probe {box 8 22 10 30}
probe {paint metal1}
probe {box 12 22 14 30}
probe {paint metal1}
probe {box 16 22 18 30}
probe {paint metal1}
probe {box 20 22 22 30}
probe {paint metal1}
probe {box 24 22 26 30}
probe {paint metal1}
probe {box 28 22 30 30}
probe {paint metal1}
probe {box 32 22 34 30}
probe {paint metal1}
probe {box 36 22 38 30}
probe {paint metal1}
probe {box 40 22 42 30}
probe {paint metal1}
probe {box 44 22 46 30}
probe {paint metal1}
probe {box 48 22 50 30}
probe {paint metal1}
probe {box 52 22 54 30}
probe {paint metal1}
probe {box 56 22 58 30}
probe {paint metal1}
probe {box 60 22 62 30}
probe {paint metal1}
probe {box 64 22 66 30}
probe {paint metal1}
probe {box 68 22 70 30}
probe {paint metal1}
probe {box 72 22 74 30}
probe {paint metal1}
probe {box 76 22 78 30}
probe {paint metal1}
probe {box 80 22 82 30}
probe {paint metal1}
probe {box 84 22 86 30}
probe {paint metal1}
probe {box 88 22 90 30}
probe {paint metal1}
probe {box 92 22 94 30}
probe {paint metal1}
probe {box 96 22 98 30}
probe {paint metal1}
probe {box 100 22 102 30}
probe {paint metal1}
probe {box 104 22 106 30}
probe {paint metal1}
probe {box 108 22 110 30}
probe {paint metal1}
probe {box 112 22 114 30}
probe {paint metal1}
probe {box 116 22 118 30}
probe {paint metal1}
probe {box 120 22 122 30}
probe {paint metal1}
probe {box 124 22 126 30}
probe {paint metal1}
probe {box 128 22 130 30}
probe {paint metal1}
probe {box 132 22 134 30}
probe {paint metal1}
probe {box 136 22 138 30}
probe {paint metal1}
probe {box 140 22 142 30}
probe {paint metal1}
probe {box 144 22 146 30}
probe {paint metal1}
probe {box 148 22 150 30}
probe {paint metal1}
probe {box 152 22 154 30}
probe {paint metal1}
probe {box 156 22 158 30}
probe {paint metal1}
probe {box 160 22 162 30}
probe {paint metal1}
probe {box 164 22 166 30}
probe {paint metal1}
probe {box 168 22 170 30}
probe {paint metal1}
probe {box 172 22 174 30}
probe {paint metal1}
probe {box 176 22 178 30}
probe {paint metal1}
probe {box 180 22 182 30}
probe {paint metal1}
probe {box 184 22 186 30}
probe {paint metal1}
probe {box 188 22 190 30}
probe {paint metal1}
probe {box 192 22 194 30}
probe {paint metal1}
probe {box 196 22 198 30}
probe {paint metal1}
probe {box 200 22 202 30}
probe {paint metal1}
probe {box 204 22 206 30}
probe {paint metal1}
probe {box 208 22 210 30}
probe {paint metal1}
probe {box 212 22 214 30}
probe {paint metal1}
probe {box 216 22 218 30}
probe {paint metal1}
probe {box 220 22 222 30}
probe {paint metal1}
probe {box 224 22 226 30}
probe {paint metal1}
probe {box 228 22 230 30}
probe {paint metal1}
probe {box 232 22 234 30}
probe {paint metal1}
probe {box 236 22 238 30}
probe {paint metal1}
probe {box 240 22 242 30}
probe {paint metal1}
probe {box 244 22 246 30}
probe {paint metal1}
probe {box 248 22 250 30}
probe {paint metal1}
probe {box 252 22 254 30}
probe {paint metal1}
probe {box 256 22 258 30}
probe {paint metal1}
probe {box 260 22 262 30}
probe {paint metal1}
probe {box 264 22 266 30}
probe {paint metal1}
probe {box 268 22 270 30}
probe {paint metal1}
probe {box 272 22 274 30}
probe {paint metal1}
probe {box 276 22 278 30}
probe {paint metal1}
probe {box 280 22 282 30}
probe {paint metal1}
probe {box 284 22 286 30}
probe {paint metal1}
probe {box 288 22 290 30}
probe {paint metal1}
probe {box 292 22 294 30}
probe {paint metal1}
probe {box 296 22 298 30}
probe {paint metal1}
probe {box 300 22 302 30}
probe {paint metal1}
probe {box 304 22 306 30}
probe {paint metal1}
probe {box 308 22 310 30}
probe {paint metal1}
probe {box 312 22 314 30}
probe {paint metal1}
probe {box 316 22 318 30}
probe {paint metal1}
probe {box 320 22 322 30}
probe {paint metal1}
probe {box 324 22 326 30}
probe {paint metal1}
probe {box 328 22 330 30}
probe {paint metal1}
probe {box 332 22 334 30}
probe {paint metal1}
probe {box 336 22 338 30}
probe {paint metal1}
probe {box 340 22 342 30}
probe {paint metal1}
probe {box 344 22 346 30}
probe {paint metal1}
probe {box 348 22 350 30}
probe {paint metal1}
probe {box 352 22 354 30}
probe {paint metal1}
probe {box 356 22 358 30}
probe {paint metal1}
probe {box 360 22 362 30}
probe {paint metal1}
probe {box 364 22 366 30}
probe {paint metal1}
probe {box 368 22 370 30}
probe {paint metal1}
probe {box 372 22 374 30}
probe {paint metal1}
probe {box 376 22 378 30}
probe {paint metal1}
probe {box 380 22 382 30}
probe {paint metal1}
probe {box 384 22 386 30}
probe {paint metal1}
probe {box 388 22 390 30}
probe {paint metal1}
probe {box 392 22 394 30}
probe {paint metal1}
probe {box 396 22 398 30}
probe {paint metal1}
probe {box 400 22 402 30}
probe {paint metal1}
probe {box 404 22 406 30}
probe {paint metal1}
probe {box 408 22 410 30}
probe {paint metal1}
probe {box 412 22 414 30}
probe {paint metal1}
probe {box 416 22 418 30}
probe {paint metal1}
probe {box 420 22 422 30}
probe {paint metal1}
probe {box 424 22 426 30}
probe {paint metal1}
probe {box 428 22 430 30}
probe {paint metal1}
probe {box 432 22 434 30}
probe {paint metal1}
probe {box 436 22 438 30}
probe {paint metal1}
probe {box 50 0 54 4}
probe {paint metal1}
probe {gds merge yes}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
