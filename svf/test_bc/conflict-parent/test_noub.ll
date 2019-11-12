; ModuleID = 'test_noub.bc'
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

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
  call void @llvm.dbg.declare(metadata i32* %i, metadata !25, metadata !16), !dbg !27
  store i32 0, i32* %i, align 4, !dbg !27
  br label %for.cond, !dbg !28

for.cond:                                         ; preds = %for.body, %entry
  %0 = load i32* %i, align 4, !dbg !29
  %cmp = icmp slt i32 %0, 256, !dbg !29
  br i1 %cmp, label %for.body, label %for.end, !dbg !33

for.body:                                         ; preds = %for.cond
  %call = call i32 @getchar(), !dbg !34
  %conv = trunc i32 %call to i8, !dbg !34
  %1 = load i32* %i, align 4, !dbg !36
  %idxprom = sext i32 %1 to i64, !dbg !37
  %arrayidx = getelementptr inbounds [256 x i8]* %array, i32 0, i64 %idxprom, !dbg !37
  store i8 %conv, i8* %arrayidx, align 1, !dbg !37
  %2 = load i32* %i, align 4, !dbg !38
  %inc = add nsw i32 %2, 1, !dbg !38
  store i32 %inc, i32* %i, align 4, !dbg !38
  br label %for.cond, !dbg !39

for.end:                                          ; preds = %for.cond
  ret i32 0, !dbg !40
}

; Function Attrs: nounwind readnone
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

declare i32 @getchar() #2

attributes #0 = { nounwind uwtable "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind readnone }
attributes #2 = { "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!12, !13}
!llvm.ident = !{!14}

!0 = !{!"0x11\0012\00clang version 3.6.0 (tags/RELEASE_360/final)\000\00\000\00\001", !1, !2, !2, !3, !2, !2} ; [ DW_TAG_compile_unit ] [/home/eric/work/svf/test_bc/conflict-parent/test.c] [DW_LANG_C99]
!1 = !{!"test.c", !"/home/eric/work/svf/test_bc/conflict-parent"}
!2 = !{}
!3 = !{!4}
!4 = !{!"0x2e\00main\00main\00\005\000\001\000\000\00256\000\005", !1, !5, !6, null, i32 (i32, i8**)* @main, null, null, !2} ; [ DW_TAG_subprogram ] [line 5] [def] [main]
!5 = !{!"0x29", !1}                               ; [ DW_TAG_file_type ] [/home/eric/work/svf/test_bc/conflict-parent/test.c]
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
!25 = !{!"0x100\00i\007\000", !26, !5, !8}        ; [ DW_TAG_auto_variable ] [i] [line 7]
!26 = !{!"0xb\007\003\000", !1, !4}               ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test.c]
!27 = !MDLocation(line: 7, column: 11, scope: !26)
!28 = !MDLocation(line: 7, column: 7, scope: !26)
!29 = !MDLocation(line: 7, column: 18, scope: !30)
!30 = !{!"0xb\002", !1, !31}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test.c]
!31 = !{!"0xb\001", !1, !32}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test.c]
!32 = !{!"0xb\007\003\001", !1, !26}              ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test.c]
!33 = !MDLocation(line: 7, column: 3, scope: !26)
!34 = !MDLocation(line: 8, column: 16, scope: !35)
!35 = !{!"0xb\007\0027\002", !1, !32}             ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test.c]
!36 = !MDLocation(line: 8, column: 11, scope: !35)
!37 = !MDLocation(line: 8, column: 5, scope: !35)
!38 = !MDLocation(line: 9, column: 5, scope: !35)
!39 = !MDLocation(line: 7, column: 3, scope: !32)
!40 = !MDLocation(line: 11, column: 3, scope: !4)
