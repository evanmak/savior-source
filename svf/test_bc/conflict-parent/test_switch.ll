; ModuleID = 'test_switch.bc'
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.src = private unnamed_addr constant [14 x i8] c"test_switch.c\00", align 1
@0 = private unnamed_addr constant { i16, i16, [13 x i8] } { i16 -1, i16 0, [13 x i8] c"'char [256]'\00" }
@1 = private unnamed_addr constant { i16, i16, [6 x i8] } { i16 0, i16 11, [6 x i8] c"'int'\00" }
@2 = private unnamed_addr global { { [14 x i8]*, i32, i32 }, { i16, i16, [13 x i8] }*, { i16, i16, [6 x i8] }* } { { [14 x i8]*, i32, i32 } { [14 x i8]* @.src, i32 10, i32 7 }, { i16, i16, [13 x i8] }* @0, { i16, i16, [6 x i8] }* @1 }
@3 = private unnamed_addr global { { [14 x i8]*, i32, i32 }, { i16, i16, [13 x i8] }*, { i16, i16, [6 x i8] }* } { { [14 x i8]*, i32, i32 } { [14 x i8]* @.src, i32 13, i32 23 }, { i16, i16, [13 x i8] }* @0, { i16, i16, [6 x i8] }* @1 }
@4 = private unnamed_addr global { { [14 x i8]*, i32, i32 }, { i16, i16, [6 x i8] }* } { { [14 x i8]*, i32, i32 } { [14 x i8]* @.src, i32 15, i32 8 }, { i16, i16, [6 x i8] }* @1 }

; Function Attrs: nounwind uwtable
define i32 @main(i32 %argc, i8** %argv) #0 {
entry:
  %retval = alloca i32, align 4
  %argc.addr = alloca i32, align 4
  %argv.addr = alloca i8**, align 8
  %array = alloca [256 x i8], align 16
  %i = alloca i32, align 4
  store i32 0, i32* %retval
  store i32 %argc, i32* %argc.addr, align 4
  call void @llvm.dbg.declare(metadata i32* %argc.addr, metadata !15, metadata !16), !dbg !17
  store i8** %argv, i8*** %argv.addr, align 8
  call void @llvm.dbg.declare(metadata i8*** %argv.addr, metadata !18, metadata !16), !dbg !19
  call void @llvm.dbg.declare(metadata [256 x i8]* %array, metadata !20, metadata !16), !dbg !24
  call void @llvm.dbg.declare(metadata i32* %i, metadata !25, metadata !16), !dbg !26
  store i32 512, i32* %i, align 4, !dbg !26
  %call = call i32 @getchar(), !dbg !27
  switch i32 %call, label %sw.default [
    i32 49, label %sw.bb
    i32 50, label %sw.bb2
  ], !dbg !28

sw.bb:                                            ; preds = %entry
  %call1 = call i32 @getchar(), !dbg !29
  %conv = trunc i32 %call1 to i8, !dbg !29
  %0 = load i32* %i, align 4, !dbg !31
  %1 = sext i32 %0 to i64, !dbg !32, !nosanitize !2
  %2 = icmp ult i64 %1, 256, !dbg !32, !nosanitize !2
  br i1 %2, label %cont, label %handler.out_of_bounds, !dbg !32, !prof !33, !nosanitize !2, !afl_edge_sanitizer !2

handler.out_of_bounds:                            ; preds = %sw.bb
  %3 = zext i32 %0 to i64, !dbg !34, !nosanitize !2
  call void @__ubsan_handle_out_of_bounds(i8* bitcast ({ { [14 x i8]*, i32, i32 }, { i16, i16, [13 x i8] }*, { i16, i16, [6 x i8] }* }* @2 to i8*), i64 %3) #5, !dbg !34, !nosanitize !2
  br label %cont, !dbg !34, !nosanitize !2

cont:                                             ; preds = %handler.out_of_bounds, %sw.bb
  %idxprom = sext i32 %0 to i64, !dbg !36
  %arrayidx.offs = add i64 %idxprom, 0, !dbg !36
  %4 = add i64 0, %arrayidx.offs, !dbg !36
  %arrayidx = getelementptr inbounds [256 x i8]* %array, i32 0, i64 %idxprom, !dbg !36
  %5 = sub i64 256, %4, !dbg !36
  %6 = icmp ult i64 256, %4, !dbg !36
  %7 = icmp ult i64 %5, 1, !dbg !36
  %8 = or i1 %6, %7, !dbg !36
  br i1 %8, label %trap, label %9

; <label>:9                                       ; preds = %cont
  store i8 %conv, i8* %arrayidx, align 1, !dbg !36
  br label %sw.epilog, !dbg !39

sw.bb2:                                           ; preds = %entry
  %10 = load i32* %i, align 4, !dbg !40
  %11 = sext i32 %10 to i64, !dbg !41, !nosanitize !2
  %12 = icmp ult i64 %11, 256, !dbg !41, !nosanitize !2
  br i1 %12, label %cont4, label %handler.out_of_bounds3, !dbg !41, !prof !33, !nosanitize !2, !afl_edge_sanitizer !2

handler.out_of_bounds3:                           ; preds = %sw.bb2
  %13 = zext i32 %10 to i64, !dbg !42, !nosanitize !2
  call void @__ubsan_handle_out_of_bounds(i8* bitcast ({ { [14 x i8]*, i32, i32 }, { i16, i16, [13 x i8] }*, { i16, i16, [6 x i8] }* }* @3 to i8*), i64 %13) #5, !dbg !42, !nosanitize !2
  br label %cont4, !dbg !42, !nosanitize !2

cont4:                                            ; preds = %handler.out_of_bounds3, %sw.bb2
  %idxprom5 = sext i32 %10 to i64, !dbg !43
  %arrayidx6.offs = add i64 %idxprom5, 0, !dbg !43
  %14 = add i64 0, %arrayidx6.offs, !dbg !43
  %arrayidx6 = getelementptr inbounds [256 x i8]* %array, i32 0, i64 %idxprom5, !dbg !43
  %15 = sub i64 256, %14, !dbg !43
  %16 = icmp ult i64 256, %14, !dbg !43
  %17 = icmp ult i64 %15, 1, !dbg !43
  %18 = or i1 %16, %17, !dbg !43
  br i1 %18, label %trap9, label %19

; <label>:19                                      ; preds = %cont4
  %20 = load i8* %arrayidx6, align 1, !dbg !43
  %conv7 = sext i8 %20 to i32, !dbg !43
  %or = or i32 -2, %conv7, !dbg !44
  store i32 %or, i32* %i, align 4, !dbg !45
  br label %sw.default, !dbg !45

sw.default:                                       ; preds = %19, %entry
  %21 = load i32* %i, align 4, !dbg !46
  %22 = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %21, i32 1), !dbg !46
  %23 = extractvalue { i32, i1 } %22, 0, !dbg !46
  %24 = extractvalue { i32, i1 } %22, 1, !dbg !46
  %25 = xor i1 %24, true, !dbg !46, !nosanitize !2
  br i1 %25, label %cont8, label %handler.add_overflow, !dbg !46, !prof !33, !nosanitize !2, !afl_edge_sanitizer !2

handler.add_overflow:                             ; preds = %sw.default
  %26 = zext i32 %21 to i64, !dbg !47, !nosanitize !2
  call void @__ubsan_handle_add_overflow(i8* bitcast ({ { [14 x i8]*, i32, i32 }, { i16, i16, [6 x i8] }* }* @4 to i8*), i64 %26, i64 1) #5, !dbg !47, !nosanitize !2
  br label %cont8, !dbg !47, !nosanitize !2

cont8:                                            ; preds = %handler.add_overflow, %sw.default
  store i32 %23, i32* %i, align 4, !dbg !48
  br label %sw.epilog, !dbg !49

sw.epilog:                                        ; preds = %cont8, %9
  ret i32 0, !dbg !50

trap:                                             ; preds = %cont
  call void @llvm.trap() #4, !dbg !36
  unreachable, !dbg !36

trap9:                                            ; preds = %cont4
  call void @llvm.trap() #4, !dbg !43
  unreachable, !dbg !43
}

; Function Attrs: nounwind readnone
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

declare i32 @getchar() #2

; Function Attrs: uwtable
declare void @__ubsan_handle_out_of_bounds(i8*, i64) #3

; Function Attrs: nounwind readnone
declare { i32, i1 } @llvm.sadd.with.overflow.i32(i32, i32) #1

; Function Attrs: uwtable
declare void @__ubsan_handle_add_overflow(i8*, i64, i64) #3

; Function Attrs: noreturn nounwind
declare void @llvm.trap() #4

attributes #0 = { nounwind uwtable "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind readnone }
attributes #2 = { "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { uwtable }
attributes #4 = { noreturn nounwind }
attributes #5 = { nounwind }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!12, !13}
!llvm.ident = !{!14}

!0 = !{!"0x11\0012\00clang version 3.6.0 (tags/RELEASE_360/final)\000\00\000\00\001", !1, !2, !2, !3, !2, !2} ; [ DW_TAG_compile_unit ] [/home/eric/work/svf/test_bc/conflict-parent/test_switch.c] [DW_LANG_C99]
!1 = !{!"test_switch.c", !"/home/eric/work/svf/test_bc/conflict-parent"}
!2 = !{}
!3 = !{!4}
!4 = !{!"0x2e\00main\00main\00\005\000\001\000\000\00256\000\005", !1, !5, !6, null, i32 (i32, i8**)* @main, null, null, !2} ; [ DW_TAG_subprogram ] [line 5] [def] [main]
!5 = !{!"0x29", !1}                               ; [ DW_TAG_file_type ] [/home/eric/work/svf/test_bc/conflict-parent/test_switch.c]
!6 = !{!"0x15\00\000\000\000\000\000\000", null, null, null, !7, null, null, null} ; [ DW_TAG_subroutine_type ] [line 0, size 0, align 0, offset 0] [from ]
!7 = !{!8, !8, !9}
!8 = !{!"0x24\00int\000\0032\0032\000\000\005", null, null} ; [ DW_TAG_base_type ] [int] [line 0, size 32, align 32, offset 0, enc DW_ATE_signed]
!9 = !{!"0xf\00\000\0064\0064\000\000", null, null, !10} ; [ DW_TAG_pointer_type ] [line 0, size 64, align 64, offset 0] [from ]
!10 = !{!"0xf\00\000\0064\0064\000\000", null, null, !11} ; [ DW_TAG_pointer_type ] [line 0, size 64, align 64, offset 0] [from char]
!11 = !{!"0x24\00char\000\008\008\000\000\006", null, null} ; [ DW_TAG_base_type ] [char] [line 0, size 8, align 8, offset 0, enc DW_ATE_signed_char]
!12 = !{i32 2, !"Dwarf Version", i32 4}
!13 = !{i32 2, !"Debug Info Version", i32 2}
!14 = !{!"clang version 3.6.0 (tags/RELEASE_360/final)"}
!15 = !{!"0x101\00argc\0016777221\000", !4, !5, !8} ; [ DW_TAG_arg_variable ] [argc] [line 5]
!16 = !{!"0x102"}                                 ; [ DW_TAG_expression ]
!17 = !MDLocation(line: 5, column: 14, scope: !4)
!18 = !{!"0x101\00argv\0033554437\000", !4, !5, !9} ; [ DW_TAG_arg_variable ] [argv] [line 5]
!19 = !MDLocation(line: 5, column: 26, scope: !4)
!20 = !{!"0x100\00array\006\000", !4, !5, !21}    ; [ DW_TAG_auto_variable ] [array] [line 6]
!21 = !{!"0x1\00\000\002048\008\000\000\000", null, null, !11, !22, null, null, null} ; [ DW_TAG_array_type ] [line 0, size 2048, align 8, offset 0] [from char]
!22 = !{!23}
!23 = !{!"0x21\000\00256"}                        ; [ DW_TAG_subrange_type ] [0, 255]
!24 = !MDLocation(line: 6, column: 8, scope: !4)
!25 = !{!"0x100\00i\007\000", !4, !5, !8}         ; [ DW_TAG_auto_variable ] [i] [line 7]
!26 = !MDLocation(line: 7, column: 7, scope: !4)
!27 = !MDLocation(line: 8, column: 10, scope: !4)
!28 = !MDLocation(line: 8, column: 3, scope: !4)
!29 = !MDLocation(line: 10, column: 18, scope: !30)
!30 = !{!"0xb\008\0020\000", !1, !4}              ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_switch.c]
!31 = !MDLocation(line: 10, column: 13, scope: !30)
!32 = !MDLocation(line: 10, column: 7, scope: !30)
!33 = !{!"branch_weights", i32 1048575, i32 1}
!34 = !MDLocation(line: 10, column: 7, scope: !35)
!35 = !{!"0xb\002", !1, !30}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_switch.c]
!36 = !MDLocation(line: 10, column: 7, scope: !37)
!37 = !{!"0xb\003", !1, !38}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_switch.c]
!38 = !{!"0xb\001", !1, !30}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_switch.c]
!39 = !MDLocation(line: 11, column: 7, scope: !30)
!40 = !MDLocation(line: 13, column: 29, scope: !30)
!41 = !MDLocation(line: 13, column: 23, scope: !30)
!42 = !MDLocation(line: 13, column: 23, scope: !35)
!43 = !MDLocation(line: 13, column: 23, scope: !37)
!44 = !MDLocation(line: 13, column: 10, scope: !30)
!45 = !MDLocation(line: 13, column: 7, scope: !30)
!46 = !MDLocation(line: 15, column: 7, scope: !30)
!47 = !MDLocation(line: 15, column: 7, scope: !35)
!48 = !MDLocation(line: 15, column: 7, scope: !37)
!49 = !MDLocation(line: 16, column: 3, scope: !30)
!50 = !MDLocation(line: 18, column: 3, scope: !4)
