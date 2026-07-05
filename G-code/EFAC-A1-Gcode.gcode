; =========================================================================
; Acknowledgements:
;   - avatorl & steven52880 for the manual filament change baseline
;   - MrMonkey1302 for the proposed current boost & homing PR
;   - Bambu Lab for the original AMS G-code logic
; =========================================================================
; EFAC-A1: External Feeder–Assisted Filament Change for Bambu Lab A1 (EXPERIMENTAL)
; Version: 0.9.7 (2026-03-28)
; Not an AMS... but kinda feels like it
; =========================================================================
; STATUS:
; G-code logic: final 
; Hardware: W.I.P.
; Version will bump up to 1.0.0 once hardware is complete and tuned
; =========================================================================
; NOTE:
; This system provides AMS-like behavior using an external feeder,
; without using the AMS port, AMS firmware, or internal printer hardware.
; =========================================================================
; Original Files:
;   - AMS reference version (A1 2025-10-31):
;      https://github.com/Hans930v/bambu-a1-g-code/blob/main/change-filament/change-filament-original.gcode
;   - Manual Filament Change v3 by avatorl & steven52880:
;      https://github.com/avatorl/bambu-a1-g-code/blob/main/change-filament/a1-manual-filament-change-v3.gcode
;	- Pull Request by MrMonkey1302:
;	   https://github.com/avatorl/bambu-a1-g-code/pull/17
;
; =========================================================================
; This file is a DERIVATIVE WORK based on the original implementation above.
;
; Modifications in this version:
;   - External feeder integration (NO AMS, firmware-safe)
;   - Removal of sound-based filament selection
;   - External feeder-controlled filament unload/load
;	- Reduced Purge
;	- AMS-like Purge
;
; This version only extends the workflow to support external hardware.
; =========================================================================
; License: MIT (see https://github.com/Hans930v/EFAC-A1/blob/main/G-code/LICENSE)
; Copyright (c) 2026 Andrzej Leszkiewicz
; Copyright (c) 2026 Hans930v


; === Initialization ===
M1007 S0 		; turn off mass estimation
G392 S0			; turn off clog detection
M204 S9000		; set print acceleration


; === Lift toolhead ===
{if toolchange_count > 1}
G17
G2 Z{max_layer_z + 0.4} I0.86 J0.86 P1 F10000 	; 0.4mm spiral ooze-catch
G1 Z{max_layer_z + 3.0} F1200                	; remaining +2.6mm to safe height
M400
{else}
G1 Z{max_layer_z + 3.0} F1200                	; single lift on first toolchange
M400
{endif}


; === Reheat nozzle ===
M106 P1 S0								; turn off part cooling fan
{if old_filament_temp > 142 && next_extruder < 255}
M104 S[old_filament_temp]	; restore old filament temperature (if above 142°C)
{endif}

; Fast move to wiper start to avoid any ooze on print
G1 X-38.2 F18000

; === Cut filament ===
M400
M412 S0                  ; disable runout detection temporarily
M400
G1 E-7 F250              ; retract 7 mm
G1 E-5 F230              ; retract 5 mm
G1 E-3 F210              ; retract 3 mm
G1 X257 F18000           ; fast move to cutter
; Boost X-axis current for cutting
M400
M17 X0.8                 ; increase X motor current
M400
; Cutter move
G1 X283.7 F400           ; max cutter move without cutter stuck error
; Alternatives:
; G1 X283 F400
; G1 X282 F400
; G1 X281 F400

; Retract after cut
G1 E-5 F1000             ; retract 5 mm after cutting
G1 X257 F6000            ; move away from cutter
; Reset X-axis current
M400
M17 X0.65                ; restore normal X motor current
M400
; === Cut filament end ===

G1 X260 F6000	; move away from cutter
M400			; wait for all moves to finish


; === Purge wiper ===
G1 X-38.2 F18000     ; fast move to wiper start
G1 X-48.2 F3000      ; slow move to wiper end
M400                 ; wait


; === Unload filament ===
G1 E3 F120			; slight push
G1 E-30 F1000		; retract 30 mm


; === Filament number communication ===
; Because apparently 4 colors wasn’t enough…
;
; next_extruder >= 0 → filament #1
; ...
; next_extruder <= 28 → filament #29
; Slots spaced in 10 mm increments from X-19 (slot 1) to X261 (slot 29).
; Higher filament number = farther right.
;
; Would you really print with 29 different filaments? (Yes, it's supported… but why???)

{if next_extruder >= 0 && next_extruder <= 19} ; if using VL53L0X
G1 X{-19 + (next_extruder * 10)} F18000 ; safe slot move
M400 S2	; 2sec wait
{else}
M400 U1		; invalid slot user pause
{endif}

; bro what are you printing???
; uncomment if using VL53L1X and comment out the 1st logic (not tested)
; {if next_extruder >= 0 && next_extruder <= 28} ; if using VL53L1X (not tested)
; G1 X{-19 + (next_extruder * 10)} F18000 ; safe slot move
; M400 S2	; 2ssec wait
; {else}
; M400 U1		; invalid slot user pause
; {endif}

; === Filament number communication end ===


; === Reset wiper & feeder encoding ===
G1 X-38.2 F18000
G1 X-48.2 F3000
M400


; === Wait for external feeder ===
; This is the part where the printer just stares into space
; while the feeder does the heavy lifting.
M1002 set_filament_type:UNKNOWN
M400 S15             ; external feeder swaps filament here
; Future: replace with shorter timed pauses
; External Feeder will:
;   - Pull out old filament
;   - Push in new filament


; === Load new filament ===
M109 S[nozzle_temperature_range_high] 	; set nozzle temp & wait
M412 S1					; re-enable filament runout detection
G1 E7 F500				; fast short initial grab (7mm)
G1 E5 F200          	; gentle grab (5mm)
G1 E3 F20				; slower load (3mm)
M400              		; short pause

G92 E0					; reset extruder
G1 E70 F300          	; purge old filament
G1 E5 F120				; complete load (total 75 mm)
M400					; wait

; Inform firmware: new filament active
G92 E0	; reset extruder
M1002 set_filament_type:{filament_type[next_extruder]}
M1002 set_filament_loaded:1
M1002 set_filament_changed:1
; === Load new filament end ===


; =========================================================================
; AMS FLUSH LOGIC (UNMODIFIED)
; =========================================================================
; This section is sacred. Do not touch.
; Seriously. Hands off. It’s like the printer’s holy scripture.
; -------------------------------------------------------------------------
; This entire flushing section is copied 1:1 from the official
; Bambu Lab AMS filament change gcode.
;
; No logic, math, constants, or sequencing have been altered.
; This is REQUIRED for:
;   - Correct slicer flush accounting (MODEL / FLUSHED / TOWER / TOTAL)
;   - Firmware recognition of AMS-like flushing behavior
;
; Do NOT optimize, refactor, or simplify this section.
; =========================================================================
{if flush_length_1 > 1}
; FLUSH_START
; always use highest temperature to flush
M400
M1002 set_filament_type:UNKNOWN
M109 S[flush_temperatures[next_extruder]]
M106 P1 S60
{if flush_length_1 > 23.7}
G1 E23.7 F{flush_volumetric_speeds[previous_extruder]/2.4053*60} ; do not need pulsatile flushing for start part
G1 E{(flush_length_1 - 23.7) * 0.02} F50
G1 E{(flush_length_1 - 23.7) * 0.23} F{flush_volumetric_speeds[previous_extruder]/2.4053*60}
G1 E{(flush_length_1 - 23.7) * 0.02} F50
G1 E{(flush_length_1 - 23.7) * 0.23} F{flush_volumetric_speeds[next_extruder]/2.4053*60}
G1 E{(flush_length_1 - 23.7) * 0.02} F50
G1 E{(flush_length_1 - 23.7) * 0.23} F{flush_volumetric_speeds[next_extruder]/2.4053*60}
G1 E{(flush_length_1 - 23.7) * 0.02} F50
G1 E{(flush_length_1 - 23.7) * 0.23} F{flush_volumetric_speeds[next_extruder]/2.4053*60}
{else}
G1 E{flush_length_1} F{flush_volumetric_speeds[previous_extruder]/2.4053*60}
{endif}
; FLUSH_END
G1 E-[old_retract_length_toolchange] F1800
G1 E[old_retract_length_toolchange] F300
M400
M1002 set_filament_type:{filament_type[next_extruder]}
{endif}

{if flush_length_1 > 45 && flush_length_2 > 1}
; WIPE
M400
M106 P1 S178
M400 S3
G1 X-38.2 F18000
G1 X-48.2 F3000
G1 X-38.2 F18000
G1 X-48.2 F3000
G1 X-38.2 F18000
G1 X-48.2 F3000
M400
M106 P1 S0
{endif}

{if flush_length_2 > 1}
M106 P1 S60
; FLUSH_START
G1 E{flush_length_2 * 0.18} F{flush_volumetric_speeds[next_extruder]/2.4053*60}
G1 E{flush_length_2 * 0.02} F50
G1 E{flush_length_2 * 0.18} F{flush_volumetric_speeds[next_extruder]/2.4053*60}
G1 E{flush_length_2 * 0.02} F50
G1 E{flush_length_2 * 0.18} F{flush_volumetric_speeds[next_extruder]/2.4053*60}
G1 E{flush_length_2 * 0.02} F50
G1 E{flush_length_2 * 0.18} F{flush_volumetric_speeds[next_extruder]/2.4053*60}
G1 E{flush_length_2 * 0.02} F50
G1 E{flush_length_2 * 0.18} F{flush_volumetric_speeds[next_extruder]/2.4053*60}
G1 E{flush_length_2 * 0.02} F50
; FLUSH_END
G1 E-[new_retract_length_toolchange] F1800
G1 E[new_retract_length_toolchange] F300
{endif}

{if flush_length_2 > 45 && flush_length_3 > 1}
; WIPE
M400
M106 P1 S178
M400 S3
G1 X-38.2 F18000
G1 X-48.2 F3000
G1 X-38.2 F18000
G1 X-48.2 F3000
G1 X-38.2 F18000
G1 X-48.2 F3000
M400
M106 P1 S0
{endif}

{if flush_length_3 > 1}
M106 P1 S60
; FLUSH_START
G1 E{flush_length_3 * 0.18} F{flush_volumetric_speeds[next_extruder]/2.4053*60}
G1 E{flush_length_3 * 0.02} F50
G1 E{flush_length_3 * 0.18} F{flush_volumetric_speeds[next_extruder]/2.4053*60}
G1 E{flush_length_3 * 0.02} F50
G1 E{flush_length_3 * 0.18} F{flush_volumetric_speeds[next_extruder]/2.4053*60}
G1 E{flush_length_3 * 0.02} F50
G1 E{flush_length_3 * 0.18} F{flush_volumetric_speeds[next_extruder]/2.4053*60}
G1 E{flush_length_3 * 0.02} F50
G1 E{flush_length_3 * 0.18} F{flush_volumetric_speeds[next_extruder]/2.4053*60}
G1 E{flush_length_3 * 0.02} F50
; FLUSH_END
G1 E-[new_retract_length_toolchange] F1800
G1 E[new_retract_length_toolchange] F300
{endif}

{if flush_length_3 > 45 && flush_length_4 > 1}
; WIPE
M400
M106 P1 S178
M400 S3
G1 X-38.2 F18000
G1 X-48.2 F3000
G1 X-38.2 F18000
G1 X-48.2 F3000
G1 X-38.2 F18000
G1 X-48.2 F3000
M400
M106 P1 S0
{endif}

{if flush_length_4 > 1}
M106 P1 S60
; FLUSH_START
G1 E{flush_length_4 * 0.18} F{flush_volumetric_speeds[next_extruder]/2.4053*60}
G1 E{flush_length_4 * 0.02} F50
G1 E{flush_length_4 * 0.18} F{flush_volumetric_speeds[next_extruder]/2.4053*60}
G1 E{flush_length_4 * 0.02} F50
G1 E{flush_length_4 * 0.18} F{flush_volumetric_speeds[next_extruder]/2.4053*60}
G1 E{flush_length_4 * 0.02} F50
G1 E{flush_length_4 * 0.18} F{flush_volumetric_speeds[next_extruder]/2.4053*60}
G1 E{flush_length_4 * 0.02} F50
G1 E{flush_length_4 * 0.18} F{flush_volumetric_speeds[next_extruder]/2.4053*60}
G1 E{flush_length_4 * 0.02} F50
; FLUSH_END
{endif}
; === END OF AMS FLUSH LOGIC (UNMODIFIED) ===


; === Wipe after purge ===
M106 P1 S204	; 80% fan speed
M400 S3			; wait 3 sec

G1 X-38.2 F18000
G1 X-48.2 F3000
G1 X-38.2 F18000
G1 X-48.2 F3000
G1 X-38.2 F18000
G1 X-48.2 F3000
M106 P1 S0
M400


; === Finalizing ===
M106 P1 S178						; 70% fan speed
M109 S[new_filament_temp]
G1 E6 F{new_filament_e_feedrate}	; compensate for spillage
M400
G92 E0 								; reset extruder
G1 E-[new_retract_length_toolchange] F1800
M400

; wipe
M106 P1 S204
M400 S3
G1 X-38.2 F18000
G1 X-48.2 F3000
G1 X-38.2 F18000
G1 X-48.2 F3000
G1 X-38.2 F18000
G1 X-48.2 F3000
M400

G1 Z{max_layer_z + 3.0} F3000
M106 P1 S0	; turn off fan


; === Restore acceleration ===
{if layer_z <= (initial_layer_print_height + 0.001)}
M204 S[initial_layer_acceleration]
{else}
M204 S[default_acceleration]
{endif}

;uncomment this if you're using clog detection, just remove ; before G392
;G392 S1		; enable clog detection
M1007 S1 	; restore mass estimation
M629		; finalize filament change lifecycle

; === Resume printing ===
