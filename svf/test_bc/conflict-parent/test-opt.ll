; ModuleID = 'test-opt.bc'
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct._IO_FILE = type { i32, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, %struct._IO_marker*, %struct._IO_FILE*, i32, i32, i64, i16, i8, [1 x i8], i8*, i64, i8*, i8*, i8*, i8*, i64, i32, [20 x i8] }
%struct._IO_marker = type { %struct._IO_marker*, %struct._IO_FILE*, i32 }

@.src = private unnamed_addr constant [7 x i8] c"test.c\00", align 1
@0 = private unnamed_addr constant { i16, i16, [13 x i8] } { i16 -1, i16 0, [13 x i8] c"'char [256]'\00" }
@1 = private unnamed_addr constant { i16, i16, [6 x i8] } { i16 0, i16 11, [6 x i8] c"'int'\00" }
@2 = private unnamed_addr global { { [7 x i8]*, i32, i32 }, { i16, i16, [13 x i8] }*, { i16, i16, [6 x i8] }* } { { [7 x i8]*, i32, i32 } { [7 x i8]* @.src, i32 8, i32 5 }, { i16, i16, [13 x i8] }* @0, { i16, i16, [6 x i8] }* @1 }
@3 = private unnamed_addr global { { [7 x i8]*, i32, i32 }, { i16, i16, [6 x i8] }* } { { [7 x i8]*, i32, i32 } { [7 x i8]* @.src, i32 9, i32 6 }, { i16, i16, [6 x i8] }* @1 }
@stdin = external global %struct._IO_FILE*

; Function Attrs: nounwind uwtable
define i32 @main(i32 %argc, i8** nocapture readnone %argv) #0 {
entry:
  tail call void @llvm.dbg.value(metadata i32 %argc, i64 0, metadata !13, metadata !29), !dbg !30
  tail call void @llvm.dbg.value(metadata i8** %argv, i64 0, metadata !14, metadata !29), !dbg !31
  tail call void @llvm.dbg.value(metadata i32 0, i64 0, metadata !19, metadata !29), !dbg !32
  br label %for.body, !dbg !33

for.body:                                         ; preds = %for.cond.backedge, %entry
  %i.04 = phi i32 [ 0, %entry ], [ %4, %for.cond.backedge ]
  %0 = load %struct._IO_FILE** @stdin, align 8, !dbg !34, !tbaa !38
  %call.i = tail call i32 @_IO_getc(%struct._IO_FILE* %0) #4, !dbg !42
  %1 = icmp ult i32 %i.04, 256, !dbg !43
  br i1 %1, label %cont, label %handler.out_of_bounds, !dbg !43, !prof !44, !nosanitize !2, !afl_edge_sanitizer !2

handler.out_of_bounds:                            ; preds = %for.body
  %2 = zext i32 %i.04 to i64, !dbg !45, !nosanitize !2
  tail call void @__ubsan_handle_out_of_bounds(i8* bitcast ({ { [7 x i8]*, i32, i32 }, { i16, i16, [13 x i8] }*, { i16, i16, [6 x i8] }* }* @2 to i8*), i64 %2) #4, !dbg !45, !nosanitize !2
  br label %cont, !dbg !45, !nosanitize !2

cont:                                             ; preds = %handler.out_of_bounds, %for.body
  %3 = tail call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %i.04, i32 1), !dbg !47
  %4 = extractvalue { i32, i1 } %3, 0, !dbg !47
  %5 = extractvalue { i32, i1 } %3, 1, !dbg !47
  br i1 %5, label %handler.add_overflow, label %for.cond.backedge, !dbg !47, !prof !48, !nosanitize !2, !afl_edge_sanitizer !2

for.cond.backedge:                                ; preds = %handler.add_overflow, %cont
  %cmp = icmp slt i32 %4, 256, !dbg !49
  br i1 %cmp, label %for.body, label %for.end, !dbg !33

handler.add_overflow:                             ; preds = %cont
  %6 = zext i32 %i.04 to i64, !dbg !52, !nosanitize !2
  tail call void @__ubsan_handle_add_overflow(i8* bitcast ({ { [7 x i8]*, i32, i32 }, { i16, i16, [6 x i8] }* }* @3 to i8*), i64 %6, i64 1) #4, !dbg !52, !nosanitize !2
  br label %for.cond.backedge, !dbg !52, !nosanitize !2

for.end:                                          ; preds = %for.cond.backedge
  ret i32 0, !dbg !53
}

; Function Attrs: uwtable
declare void @__ubsan_handle_out_of_bounds(i8*, i64) #1

; Function Attrs: nounwind readnone
declare { i32, i1 } @llvm.sadd.with.overflow.i32(i32, i32) #2

; Function Attrs: uwtable
declare void @__ubsan_handle_add_overflow(i8*, i64, i64) #1

; Function Attrs: nounwind
declare i32 @_IO_getc(%struct._IO_FILE* nocapture) #3

; Function Attrs: nounwind readnone
declare void @llvm.dbg.value(metadata, i64, metadata, metadata) #2

attributes #0 = { nounwind uwtable "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { uwtable }
attributes #2 = { nounwind readnone }
attributes #3 = { nounwind "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #4 = { nounwind }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!26, !27}
!llvm.ident = !{!28}

!0 = !{!"0x11\0012\00clang version 3.6.0 (tags/RELEASE_360/final)\001\00\000\00\001", !1, !2, !2, !3, !2, !2} ; [ DW_TAG_compile_unit ] [/home/eric/work/svf/test_bc/conflict-parent/test.c] [DW_LANG_C99]
!1 = !{!"test.c", !"/home/eric/work/svf/test_bc/conflict-parent"}
!2 = !{}
!3 = !{!4, !21}
!4 = !{!"0x2e\00main\00main\00\005\000\001\000\000\00256\001\005", !1, !5, !6, null, i32 (i32, i8**)* @main, null, null, !12} ; [ DW_TAG_subprogram ] [line 5] [def] [main]
!5 = !{!"0x29", !1}                               ; [ DW_TAG_file_type ] [/home/eric/work/svf/test_bc/conflict-parent/test.c]
!6 = !{!"0x15\00\000\000\000\000\000\000", null, null, null, !7, null, null, null} ; [ DW_TAG_subroutine_type ] [line 0, size 0, align 0, offset 0] [from ]
!7 = !{!8, !8, !9}
!8 = !{!"0x24\00int\000\0032\0032\000\000\005", null, null} ; [ DW_TAG_base_type ] [int] [line 0, size 32, align 32, offset 0, enc DW_ATE_signed]
!9 = !{!"0xf\00\000\0064\0064\000\000", null, null, !10} ; [ DW_TAG_pointer_type ] [line 0, size 64, align 64, offset 0] [from ]
!10 = !{!"0xf\00\000\0064\0064\000\000", null, null, !11} ; [ DW_TAG_pointer_type ] [line 0, size 64, align 64, offset 0] [from char]
!11 = !{!"0x24\00char\000\008\008\000\000\006", null, null} ; [ DW_TAG_base_type ] [char] [line 0, size 8, align 8, offset 0, enc DW_ATE_signed_char]
!12 = !{!13, !14, !15, !19}
!13 = !{!"0x101\00argc\0016777221\000", !4, !5, !8} ; [ DW_TAG_arg_variable ] [argc] [line 5]
!14 = !{!"0x101\00argv\0033554437\000", !4, !5, !9} ; [ DW_TAG_arg_variable ] [argv] [line 5]
!15 = !{!"0x100\00array\006\000", !4, !5, !16}    ; [ DW_TAG_auto_variable ] [array] [line 6]
!16 = !{!"0x1\00\000\002048\008\000\000\000", null, null, !11, !17, null, null, null} ; [ DW_TAG_array_type ] [line 0, size 2048, align 8, offset 0] [from char]
!17 = !{!18}
!18 = !{!"0x21\000\00256"}                        ; [ DW_TAG_subrange_type ] [0, 255]
!19 = !{!"0x100\00i\007\000", !20, !5, !8}        ; [ DW_TAG_auto_variable ] [i] [line 7]
!20 = !{!"0xb\007\003\000", !1, !4}               ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test.c]
!21 = !{!"0x2e\00getchar\00getchar\00\0044\000\001\000\000\00256\001\0045", !22, !23, !24, null, null, null, null, !2} ; [ DW_TAG_subprogram ] [line 44] [def] [scope 45] [getchar]
!22 = !{!"/usr/include/x86_64-linux-gnu/bits/stdio.h", !"/home/eric/work/svf/test_bc/conflict-parent"}
!23 = !{!"0x29", !22}                             ; [ DW_TAG_file_type ] [/home/eric/work/svf/test_bc/conflict-parent//usr/include/x86_64-linux-gnu/bits/stdio.h]
!24 = !{!"0x15\00\000\000\000\000\000\000", null, null, null, !25, null, null, null} ; [ DW_TAG_subroutine_type ] [line 0, size 0, align 0, offset 0] [from ]
!25 = !{!8}
!26 = !{i32 2, !"Dwarf Version", i32 4}
!27 = !{i32 2, !"Debug Info Version", i32 2}
!28 = !{!"clang version 3.6.0 (tags/RELEASE_360/final)"}
!29 = !{!"0x102"}                                 ; [ DW_TAG_expression ]
!30 = !MDLocation(line: 5, column: 14, scope: !4)
!31 = !MDLocation(line: 5, column: 26, scope: !4)
!32 = !MDLocation(line: 7, column: 11, scope: !20)
!33 = !MDLocation(line: 7, column: 3, scope: !20)
!34 = !MDLocation(line: 46, column: 20, scope: !21, inlinedAt: !35)
!35 = !MDLocation(line: 8, column: 16, scope: !36)
!36 = !{!"0xb\007\0027\002", !1, !37}             ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test.c]
!37 = !{!"0xb\007\003\001", !1, !20}              ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test.c]
!38 = !{!39, !39, i64 0}
!39 = !{!"any pointer", !40, i64 0}
!40 = !{!"omnipotent char", !41, i64 0}
!41 = !{!"Simple C/C++ TBAA"}
!42 = !MDLocation(line: 46, column: 10, scope: !21, inlinedAt: !35)
!43 = !MDLocation(line: 8, column: 5, scope: !36)
!44 = !{!"branch_weights", i32 1048575, i32 1}
!45 = !MDLocation(line: 8, column: 5, scope: !46)
!46 = !{!"0xb\002", !1, !36}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test.c]
!47 = !MDLocation(line: 9, column: 5, scope: !36)
!48 = !{!"branch_weights", i32 1, i32 1048575}
!49 = !MDLocation(line: 7, column: 18, scope: !50)
!50 = !{!"0xb\002", !1, !51}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test.c]
!51 = !{!"0xb\001", !1, !37}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test.c]
!52 = !MDLocation(line: 9, column: 5, scope: !46)
!53 = !MDLocation(line: 12, column: 1, scope: !4)
