; ModuleID = 'test_bad3.bc'
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.src = private unnamed_addr constant [11 x i8] c"test_bad.c\00", align 1
@0 = private unnamed_addr constant { i16, i16, [13 x i8] } { i16 -1, i16 0, [13 x i8] c"'char [256]'\00" }
@1 = private unnamed_addr constant { i16, i16, [6 x i8] } { i16 0, i16 11, [6 x i8] c"'int'\00" }
@2 = private unnamed_addr global { { [11 x i8]*, i32, i32 }, { i16, i16, [13 x i8] }*, { i16, i16, [6 x i8] }* } { { [11 x i8]*, i32, i32 } { [11 x i8]* @.src, i32 9, i32 7 }, { i16, i16, [13 x i8] }* @0, { i16, i16, [6 x i8] }* @1 }
@3 = private unnamed_addr global { { [11 x i8]*, i32, i32 }, { i16, i16, [13 x i8] }*, { i16, i16, [6 x i8] }* } { { [11 x i8]*, i32, i32 } { [11 x i8]* @.src, i32 11, i32 23 }, { i16, i16, [13 x i8] }* @0, { i16, i16, [6 x i8] }* @1 }

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
  %0 = load i32* %i, align 4, !dbg !27
  %cmp = icmp slt i32 %0, 256, !dbg !27
  br i1 %cmp, label %if.then, label %if.else, !dbg !29

if.then:                                          ; preds = %entry
  %call = call i32 @getchar(), !dbg !30
  %conv = trunc i32 %call to i8, !dbg !30
  %1 = extractvalue { i32, i1 } zeroinitializer, 0
  %2 = sext i32 %1 to i64, !dbg !33, !nosanitize !2
  %3 = icmp ult i64 %2, 256, !dbg !33, !nosanitize !2
  br i1 %3, label %cont, label %handler.out_of_bounds, !dbg !33, !prof !34, !nosanitize !2, !afl_edge_sanitizer !2

handler.out_of_bounds:                            ; preds = %if.then
  %4 = zext i32 %1 to i64, !dbg !35, !nosanitize !2
  call void @__ubsan_handle_out_of_bounds(i8* bitcast ({ { [11 x i8]*, i32, i32 }, { i16, i16, [13 x i8] }*, { i16, i16, [6 x i8] }* }* @2 to i8*), i64 %4) #5, !dbg !35, !nosanitize !2
  br label %cont, !dbg !35, !nosanitize !2

cont:                                             ; preds = %handler.out_of_bounds, %if.then
  %idxprom = sext i32 %1 to i64, !dbg !37
  %arrayidx.offs = add i64 %idxprom, 0, !dbg !37
  %5 = add i64 0, %arrayidx.offs, !dbg !37
  %arrayidx = getelementptr inbounds [256 x i8]* %array, i32 0, i64 %idxprom, !dbg !37
  %6 = sub i64 256, %5, !dbg !37
  %7 = icmp ult i64 256, %5, !dbg !37
  %8 = icmp ult i64 %6, 1, !dbg !37
  %9 = or i1 %7, %8, !dbg !37
  br i1 %9, label %trap, label %10

; <label>:10                                      ; preds = %cont
  store i8 %conv, i8* %arrayidx, align 1, !dbg !37
  br label %if.end, !dbg !40

if.else:                                          ; preds = %entry
  %11 = load i32* %i, align 4, !dbg !41
  %12 = sext i32 %11 to i64, !dbg !43, !nosanitize !2
  %13 = icmp ult i64 %12, 256, !dbg !43, !nosanitize !2
  br i1 %13, label %cont2, label %handler.out_of_bounds1, !dbg !43, !prof !34, !nosanitize !2, !afl_edge_sanitizer !2

handler.out_of_bounds1:                           ; preds = %if.else
  %14 = zext i32 %11 to i64, !dbg !44, !nosanitize !2
  call void @__ubsan_handle_out_of_bounds(i8* bitcast ({ { [11 x i8]*, i32, i32 }, { i16, i16, [13 x i8] }*, { i16, i16, [6 x i8] }* }* @3 to i8*), i64 %14) #5, !dbg !44, !nosanitize !2
  br label %cont2, !dbg !44, !nosanitize !2

cont2:                                            ; preds = %handler.out_of_bounds1, %if.else
  %idxprom3 = sext i32 %11 to i64, !dbg !46
  %arrayidx4.offs = add i64 %idxprom3, 0, !dbg !46
  %15 = add i64 0, %arrayidx4.offs, !dbg !46
  %arrayidx4 = getelementptr inbounds [256 x i8]* %array, i32 0, i64 %idxprom3, !dbg !46
  %16 = sub i64 256, %15, !dbg !46
  %17 = icmp ult i64 256, %15, !dbg !46
  %18 = icmp ult i64 %16, 1, !dbg !46
  %19 = or i1 %17, %18, !dbg !46
  br i1 %19, label %trap6, label %20

; <label>:20                                      ; preds = %cont2
  %21 = load i8* %arrayidx4, align 1, !dbg !46
  %conv5 = sext i8 %21 to i32, !dbg !46
  %or = or i32 -2, %conv5, !dbg !49
  store i32 %or, i32* %i, align 4, !dbg !50
  br label %if.end

if.end:                                           ; preds = %20, %10
  ret i32 0, !dbg !51

trap:                                             ; preds = %cont
  call void @llvm.trap() #4, !dbg !37
  unreachable, !dbg !37

trap6:                                            ; preds = %cont2
  call void @llvm.trap() #4, !dbg !46
  unreachable, !dbg !46
}

; Function Attrs: nounwind readnone
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

declare i32 @getchar() #2

; Function Attrs: uwtable
declare void @__ubsan_handle_out_of_bounds(i8*, i64) #3

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

!0 = !{!"0x11\0012\00clang version 3.6.0 (tags/RELEASE_360/final)\000\00\000\00\001", !1, !2, !2, !3, !2, !2} ; [ DW_TAG_compile_unit ] [/home/eric/work/svf/test_bc/conflict-parent/test_bad.c] [DW_LANG_C99]
!1 = !{!"test_bad.c", !"/home/eric/work/svf/test_bc/conflict-parent"}
!2 = !{}
!3 = !{!4}
!4 = !{!"0x2e\00main\00main\00\005\000\001\000\000\00256\000\005", !1, !5, !6, null, i32 (i32, i8**)* @main, null, null, !2} ; [ DW_TAG_subprogram ] [line 5] [def] [main]
!5 = !{!"0x29", !1}                               ; [ DW_TAG_file_type ] [/home/eric/work/svf/test_bc/conflict-parent/test_bad.c]
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
!27 = !MDLocation(line: 8, column: 7, scope: !28)
!28 = !{!"0xb\008\007\000", !1, !4}               ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_bad.c]
!29 = !MDLocation(line: 8, column: 7, scope: !4)
!30 = !MDLocation(line: 9, column: 18, scope: !31)
!31 = !{!"0xb\008\0015\001", !1, !28}             ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_bad.c]
!32 = !MDLocation(line: 9, column: 13, scope: !31)
!33 = !MDLocation(line: 9, column: 7, scope: !31)
!34 = !{!"branch_weights", i32 1048575, i32 1}
!35 = !MDLocation(line: 9, column: 7, scope: !36)
!36 = !{!"0xb\002", !1, !31}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_bad.c]
!37 = !MDLocation(line: 9, column: 7, scope: !38)
!38 = !{!"0xb\003", !1, !39}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_bad.c]
!39 = !{!"0xb\001", !1, !31}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_bad.c]
!40 = !MDLocation(line: 10, column: 3, scope: !31)
!41 = !MDLocation(line: 11, column: 29, scope: !42)
!42 = !{!"0xb\0010\008\002", !1, !28}             ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_bad.c]
!43 = !MDLocation(line: 11, column: 23, scope: !42)
!44 = !MDLocation(line: 11, column: 23, scope: !45)
!45 = !{!"0xb\002", !1, !42}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_bad.c]
!46 = !MDLocation(line: 11, column: 23, scope: !47)
!47 = !{!"0xb\003", !1, !48}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_bad.c]
!48 = !{!"0xb\001", !1, !42}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_bad.c]
!49 = !MDLocation(line: 11, column: 10, scope: !42)
!50 = !MDLocation(line: 11, column: 7, scope: !42)
!51 = !MDLocation(line: 14, column: 3, scope: !4)
