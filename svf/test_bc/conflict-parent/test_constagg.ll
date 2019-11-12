; ModuleID = 'test_constagg.bc'
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.A = type { i32, i8, i32 }

@main.a = private unnamed_addr constant %struct.A { i32 1, i8 99, i32 2 }, align 4
@.src = private unnamed_addr constant [16 x i8] c"test_constagg.c\00", align 1
@0 = private unnamed_addr constant { i16, i16, [13 x i8] } { i16 -1, i16 0, [13 x i8] c"'char [256]'\00" }
@1 = private unnamed_addr constant { i16, i16, [6 x i8] } { i16 0, i16 11, [6 x i8] c"'int'\00" }
@2 = private unnamed_addr global { { [16 x i8]*, i32, i32 }, { i16, i16, [13 x i8] }*, { i16, i16, [6 x i8] }* } { { [16 x i8]*, i32, i32 } { [16 x i8]* @.src, i32 16, i32 7 }, { i16, i16, [13 x i8] }* @0, { i16, i16, [6 x i8] }* @1 }
@3 = private unnamed_addr global { { [16 x i8]*, i32, i32 }, { i16, i16, [6 x i8] }* } { { [16 x i8]*, i32, i32 } { [16 x i8]* @.src, i32 18, i32 14 }, { i16, i16, [6 x i8] }* @1 }

; Function Attrs: nounwind uwtable
define i32 @main(i32 %argc, i8** %argv) #0 {
entry:
  %retval = alloca i32, align 4
  %argc.addr = alloca i32, align 4
  %argv.addr = alloca i8**, align 8
  %array = alloca [256 x i8], align 16
  %a = alloca %struct.A, align 4
  %i = alloca i32, align 4
  store i32 0, i32* %retval
  store i32 %argc, i32* %argc.addr, align 4
  call void @llvm.dbg.declare(metadata i32* %argc.addr, metadata !15, metadata !16), !dbg !17
  store i8** %argv, i8*** %argv.addr, align 8
  call void @llvm.dbg.declare(metadata i8*** %argv.addr, metadata !18, metadata !16), !dbg !19
  call void @llvm.dbg.declare(metadata [256 x i8]* %array, metadata !20, metadata !16), !dbg !24
  call void @llvm.dbg.declare(metadata %struct.A* %a, metadata !25, metadata !16), !dbg !31
  %0 = bitcast %struct.A* %a to i8*, !dbg !31
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* %0, i8* bitcast (%struct.A* @main.a to i8*), i64 12, i32 4, i1 false), !dbg !31
  call void @llvm.dbg.declare(metadata i32* %i, metadata !32, metadata !16), !dbg !33
  store i32 512, i32* %i, align 4, !dbg !33
  %1 = load i32* %i, align 4, !dbg !34
  %cmp = icmp slt i32 %1, 256, !dbg !34
  br i1 %cmp, label %if.then, label %if.else, !dbg !36

if.then:                                          ; preds = %entry
  %call = call i32 @getchar(), !dbg !37
  %conv = trunc i32 %call to i8, !dbg !37
  %2 = load i32* %i, align 4, !dbg !39
  %3 = sext i32 %2 to i64, !dbg !40, !nosanitize !2
  %4 = icmp ult i64 %3, 256, !dbg !40, !nosanitize !2
  br i1 %4, label %cont, label %handler.out_of_bounds, !dbg !40, !prof !41, !nosanitize !2, !afl_edge_sanitizer !2

handler.out_of_bounds:                            ; preds = %if.then
  %5 = zext i32 %2 to i64, !dbg !42, !nosanitize !2
  call void @__ubsan_handle_out_of_bounds(i8* bitcast ({ { [16 x i8]*, i32, i32 }, { i16, i16, [13 x i8] }*, { i16, i16, [6 x i8] }* }* @2 to i8*), i64 %5) #2, !dbg !42, !nosanitize !2
  br label %cont, !dbg !42, !nosanitize !2

cont:                                             ; preds = %handler.out_of_bounds, %if.then
  %idxprom = sext i32 %2 to i64, !dbg !44
  %arrayidx.offs = add i64 %idxprom, 0, !dbg !44
  %6 = add i64 0, %arrayidx.offs, !dbg !44
  %arrayidx = getelementptr inbounds [256 x i8]* %array, i32 0, i64 %idxprom, !dbg !44
  %7 = sub i64 256, %6, !dbg !44
  %8 = icmp ult i64 256, %6, !dbg !44
  %9 = icmp ult i64 %7, 1, !dbg !44
  %10 = or i1 %8, %9, !dbg !44
  br i1 %10, label %trap, label %11

; <label>:11                                      ; preds = %cont
  store i8 %conv, i8* %arrayidx, align 1, !dbg !44
  br label %if.end, !dbg !47

if.else:                                          ; preds = %entry
  %c = getelementptr inbounds %struct.A* %a, i32 0, i32 2, !dbg !48
  %12 = load i32* %c, align 4, !dbg !48
  %13 = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %12, i32 1), !dbg !48
  %14 = extractvalue { i32, i1 } %13, 0, !dbg !48
  %15 = extractvalue { i32, i1 } %13, 1, !dbg !48
  %16 = xor i1 %15, true, !dbg !48, !nosanitize !2
  br i1 %16, label %cont1, label %handler.add_overflow, !dbg !48, !prof !41, !nosanitize !2, !afl_edge_sanitizer !2

handler.add_overflow:                             ; preds = %if.else
  %17 = zext i32 %12 to i64, !dbg !50, !nosanitize !2
  call void @__ubsan_handle_add_overflow(i8* bitcast ({ { [16 x i8]*, i32, i32 }, { i16, i16, [6 x i8] }* }* @3 to i8*), i64 %17, i64 1) #2, !dbg !50, !nosanitize !2
  br label %cont1, !dbg !50, !nosanitize !2

cont1:                                            ; preds = %handler.add_overflow, %if.else
  store i32 %14, i32* %i, align 4, !dbg !52
  br label %if.end

if.end:                                           ; preds = %cont1, %11
  ret i32 0, !dbg !55

trap:                                             ; preds = %cont
  call void @llvm.trap() #5, !dbg !44
  unreachable, !dbg !44
}

; Function Attrs: nounwind readnone
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

; Function Attrs: nounwind
declare void @llvm.memcpy.p0i8.p0i8.i64(i8* nocapture, i8* nocapture readonly, i64, i32, i1) #2

declare i32 @getchar() #3

; Function Attrs: uwtable
declare void @__ubsan_handle_out_of_bounds(i8*, i64) #4

; Function Attrs: nounwind readnone
declare { i32, i1 } @llvm.sadd.with.overflow.i32(i32, i32) #1

; Function Attrs: uwtable
declare void @__ubsan_handle_add_overflow(i8*, i64, i64) #4

; Function Attrs: noreturn nounwind
declare void @llvm.trap() #5

attributes #0 = { nounwind uwtable "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind readnone }
attributes #2 = { nounwind }
attributes #3 = { "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #4 = { uwtable }
attributes #5 = { noreturn nounwind }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!12, !13}
!llvm.ident = !{!14}

!0 = !{!"0x11\0012\00clang version 3.6.0 (tags/RELEASE_360/final)\000\00\000\00\001", !1, !2, !2, !3, !2, !2} ; [ DW_TAG_compile_unit ] [/home/eric/work/svf/test_bc/conflict-parent/test_constagg.c] [DW_LANG_C99]
!1 = !{!"test_constagg.c", !"/home/eric/work/svf/test_bc/conflict-parent"}
!2 = !{}
!3 = !{!4}
!4 = !{!"0x2e\00main\00main\00\0011\000\001\000\000\00256\000\0011", !1, !5, !6, null, i32 (i32, i8**)* @main, null, null, !2} ; [ DW_TAG_subprogram ] [line 11] [def] [main]
!5 = !{!"0x29", !1}                               ; [ DW_TAG_file_type ] [/home/eric/work/svf/test_bc/conflict-parent/test_constagg.c]
!6 = !{!"0x15\00\000\000\000\000\000\000", null, null, null, !7, null, null, null} ; [ DW_TAG_subroutine_type ] [line 0, size 0, align 0, offset 0] [from ]
!7 = !{!8, !8, !9}
!8 = !{!"0x24\00int\000\0032\0032\000\000\005", null, null} ; [ DW_TAG_base_type ] [int] [line 0, size 32, align 32, offset 0, enc DW_ATE_signed]
!9 = !{!"0xf\00\000\0064\0064\000\000", null, null, !10} ; [ DW_TAG_pointer_type ] [line 0, size 64, align 64, offset 0] [from ]
!10 = !{!"0xf\00\000\0064\0064\000\000", null, null, !11} ; [ DW_TAG_pointer_type ] [line 0, size 64, align 64, offset 0] [from char]
!11 = !{!"0x24\00char\000\008\008\000\000\006", null, null} ; [ DW_TAG_base_type ] [char] [line 0, size 8, align 8, offset 0, enc DW_ATE_signed_char]
!12 = !{i32 2, !"Dwarf Version", i32 4}
!13 = !{i32 2, !"Debug Info Version", i32 2}
!14 = !{!"clang version 3.6.0 (tags/RELEASE_360/final)"}
!15 = !{!"0x101\00argc\0016777227\000", !4, !5, !8} ; [ DW_TAG_arg_variable ] [argc] [line 11]
!16 = !{!"0x102"}                                 ; [ DW_TAG_expression ]
!17 = !MDLocation(line: 11, column: 14, scope: !4)
!18 = !{!"0x101\00argv\0033554443\000", !4, !5, !9} ; [ DW_TAG_arg_variable ] [argv] [line 11]
!19 = !MDLocation(line: 11, column: 26, scope: !4)
!20 = !{!"0x100\00array\0012\000", !4, !5, !21}   ; [ DW_TAG_auto_variable ] [array] [line 12]
!21 = !{!"0x1\00\000\002048\008\000\000\000", null, null, !11, !22, null, null, null} ; [ DW_TAG_array_type ] [line 0, size 2048, align 8, offset 0] [from char]
!22 = !{!23}
!23 = !{!"0x21\000\00256"}                        ; [ DW_TAG_subrange_type ] [0, 255]
!24 = !MDLocation(line: 12, column: 8, scope: !4)
!25 = !{!"0x100\00a\0013\000", !4, !5, !26}       ; [ DW_TAG_auto_variable ] [a] [line 13]
!26 = !{!"0x13\00A\004\0096\0032\000\000\000", !1, null, null, !27, null, null, null} ; [ DW_TAG_structure_type ] [A] [line 4, size 96, align 32, offset 0] [def] [from ]
!27 = !{!28, !29, !30}
!28 = !{!"0xd\00a\005\0032\0032\000\000", !1, !26, !8} ; [ DW_TAG_member ] [a] [line 5, size 32, align 32, offset 0] [from int]
!29 = !{!"0xd\00b\006\008\008\0032\000", !1, !26, !11} ; [ DW_TAG_member ] [b] [line 6, size 8, align 8, offset 32] [from char]
!30 = !{!"0xd\00c\007\0032\0032\0064\000", !1, !26, !8} ; [ DW_TAG_member ] [c] [line 7, size 32, align 32, offset 64] [from int]
!31 = !MDLocation(line: 13, column: 12, scope: !4)
!32 = !{!"0x100\00i\0014\000", !4, !5, !8}        ; [ DW_TAG_auto_variable ] [i] [line 14]
!33 = !MDLocation(line: 14, column: 7, scope: !4)
!34 = !MDLocation(line: 15, column: 7, scope: !35)
!35 = !{!"0xb\0015\007\000", !1, !4}              ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_constagg.c]
!36 = !MDLocation(line: 15, column: 7, scope: !4)
!37 = !MDLocation(line: 16, column: 18, scope: !38)
!38 = !{!"0xb\0015\0015\001", !1, !35}            ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_constagg.c]
!39 = !MDLocation(line: 16, column: 13, scope: !38)
!40 = !MDLocation(line: 16, column: 7, scope: !38)
!41 = !{!"branch_weights", i32 1048575, i32 1}
!42 = !MDLocation(line: 16, column: 7, scope: !43)
!43 = !{!"0xb\002", !1, !38}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_constagg.c]
!44 = !MDLocation(line: 16, column: 7, scope: !45)
!45 = !{!"0xb\003", !1, !46}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_constagg.c]
!46 = !{!"0xb\001", !1, !38}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_constagg.c]
!47 = !MDLocation(line: 17, column: 3, scope: !38)
!48 = !MDLocation(line: 18, column: 10, scope: !49)
!49 = !{!"0xb\0017\008\002", !1, !35}             ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_constagg.c]
!50 = !MDLocation(line: 18, column: 10, scope: !51)
!51 = !{!"0xb\002", !1, !49}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_constagg.c]
!52 = !MDLocation(line: 18, column: 7, scope: !53)
!53 = !{!"0xb\003", !1, !54}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_constagg.c]
!54 = !{!"0xb\001", !1, !49}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_constagg.c]
!55 = !MDLocation(line: 21, column: 3, scope: !4)
