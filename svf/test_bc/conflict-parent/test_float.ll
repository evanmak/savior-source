; ModuleID = 'test_float.bc'
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.src = private unnamed_addr constant [13 x i8] c"test_float.c\00", align 1
@0 = private unnamed_addr constant { i16, i16, [6 x i8] } { i16 0, i16 11, [6 x i8] c"'int'\00" }
@1 = private unnamed_addr global { { [13 x i8]*, i32, i32 }, { i16, i16, [6 x i8] }* } { { [13 x i8]*, i32, i32 } { [13 x i8]* @.src, i32 15, i32 9 }, { i16, i16, [6 x i8] }* @0 }
@2 = private unnamed_addr global { { [13 x i8]*, i32, i32 }, { i16, i16, [6 x i8] }* } { { [13 x i8]*, i32, i32 } { [13 x i8]* @.src, i32 15, i32 15 }, { i16, i16, [6 x i8] }* @0 }
@3 = private unnamed_addr global { { [13 x i8]*, i32, i32 }, { i16, i16, [6 x i8] }* } { { [13 x i8]*, i32, i32 } { [13 x i8]* @.src, i32 16, i32 15 }, { i16, i16, [6 x i8] }* @0 }
@4 = private unnamed_addr constant { i16, i16, [13 x i8] } { i16 -1, i16 0, [13 x i8] c"'char [256]'\00" }
@5 = private unnamed_addr global { { [13 x i8]*, i32, i32 }, { i16, i16, [13 x i8] }*, { i16, i16, [6 x i8] }* } { { [13 x i8]*, i32, i32 } { [13 x i8]* @.src, i32 17, i32 11 }, { i16, i16, [13 x i8] }* @4, { i16, i16, [6 x i8] }* @0 }
@6 = private unnamed_addr global { { [13 x i8]*, i32, i32 }, { i16, i16, [6 x i8] }* } { { [13 x i8]*, i32, i32 } { [13 x i8]* @.src, i32 19, i32 19 }, { i16, i16, [6 x i8] }* @0 }

; Function Attrs: nounwind uwtable
define i32 @main(i32 %argc, i8** %argv) #0 {
entry:
  %retval = alloca i32, align 4
  %argc.addr = alloca i32, align 4
  %argv.addr = alloca i8**, align 8
  %array = alloca [256 x i8], align 16
  %i = alloca i32, align 4
  %z = alloca double, align 8
  %j = alloca i32, align 4
  store i32 0, i32* %retval
  store i32 %argc, i32* %argc.addr, align 4
  call void @llvm.dbg.declare(metadata i32* %argc.addr, metadata !16, metadata !17), !dbg !18
  store i8** %argv, i8*** %argv.addr, align 8
  call void @llvm.dbg.declare(metadata i8*** %argv.addr, metadata !19, metadata !17), !dbg !20
  call void @llvm.dbg.declare(metadata [256 x i8]* %array, metadata !21, metadata !17), !dbg !25
  call void @llvm.dbg.declare(metadata i32* %i, metadata !26, metadata !17), !dbg !27
  store i32 512, i32* %i, align 4, !dbg !27
  call void @llvm.dbg.declare(metadata double* %z, metadata !28, metadata !17), !dbg !30
  store double 5.120000e+02, double* %z, align 8, !dbg !30
  call void @llvm.dbg.declare(metadata i32* %j, metadata !31, metadata !17), !dbg !32
  store i32 123, i32* %j, align 4, !dbg !32
  %0 = load i32* %j, align 4, !dbg !33
  %1 = load i32* %i, align 4, !dbg !35
  %2 = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %0, i32 %1), !dbg !33
  %3 = extractvalue { i32, i1 } %2, 0, !dbg !33
  %4 = extractvalue { i32, i1 } %2, 1, !dbg !33
  %5 = xor i1 %4, true, !dbg !33, !nosanitize !2
  br i1 %5, label %cont, label %handler.add_overflow, !dbg !33, !prof !36, !nosanitize !2, !afl_edge_sanitizer !2

handler.add_overflow:                             ; preds = %entry
  %6 = zext i32 %0 to i64, !dbg !37, !nosanitize !2
  %7 = zext i32 %1 to i64, !dbg !37, !nosanitize !2
  call void @__ubsan_handle_add_overflow(i8* bitcast ({ { [13 x i8]*, i32, i32 }, { i16, i16, [6 x i8] }* }* @1 to i8*), i64 %6, i64 %7) #5, !dbg !37, !nosanitize !2
  br label %cont, !dbg !37, !nosanitize !2

cont:                                             ; preds = %handler.add_overflow, %entry
  %8 = call { i32, i1 } @llvm.ssub.with.overflow.i32(i32 0, i32 1), !dbg !39
  %9 = extractvalue { i32, i1 } %8, 0, !dbg !39
  %10 = extractvalue { i32, i1 } %8, 1, !dbg !39
  %11 = xor i1 %10, true, !dbg !39, !nosanitize !2
  br i1 %11, label %cont1, label %handler.negate_overflow, !dbg !39, !prof !36, !nosanitize !2, !afl_edge_sanitizer !2

handler.negate_overflow:                          ; preds = %cont
  call void @__ubsan_handle_negate_overflow(i8* bitcast ({ { [13 x i8]*, i32, i32 }, { i16, i16, [6 x i8] }* }* @2 to i8*), i64 1) #5, !dbg !42, !nosanitize !2
  br label %cont1, !dbg !42, !nosanitize !2

cont1:                                            ; preds = %handler.negate_overflow, %cont
  %cmp = icmp sgt i32 %3, %9, !dbg !44
  br i1 %cmp, label %if.then, label %if.end9, !dbg !47

if.then:                                          ; preds = %cont1
  %12 = load i32* %i, align 4, !dbg !48
  %13 = call { i32, i1 } @llvm.ssub.with.overflow.i32(i32 0, i32 1), !dbg !51
  %14 = extractvalue { i32, i1 } %13, 0, !dbg !51
  %15 = extractvalue { i32, i1 } %13, 1, !dbg !51
  %16 = xor i1 %15, true, !dbg !51, !nosanitize !2
  br i1 %16, label %cont3, label %handler.negate_overflow2, !dbg !51, !prof !36, !nosanitize !2, !afl_edge_sanitizer !2

handler.negate_overflow2:                         ; preds = %if.then
  call void @__ubsan_handle_negate_overflow(i8* bitcast ({ { [13 x i8]*, i32, i32 }, { i16, i16, [6 x i8] }* }* @3 to i8*), i64 1) #5, !dbg !52, !nosanitize !2
  br label %cont3, !dbg !52, !nosanitize !2

cont3:                                            ; preds = %handler.negate_overflow2, %if.then
  %cmp4 = icmp sgt i32 %12, -1, !dbg !54
  br i1 %cmp4, label %if.then5, label %if.else, !dbg !57

if.then5:                                         ; preds = %cont3
  %call = call i32 @getchar(), !dbg !58
  %conv = trunc i32 %call to i8, !dbg !58
  %17 = load i32* %i, align 4, !dbg !60
  %18 = sext i32 %17 to i64, !dbg !61, !nosanitize !2
  %19 = icmp ult i64 %18, 256, !dbg !61, !nosanitize !2
  br i1 %19, label %cont6, label %handler.out_of_bounds, !dbg !61, !prof !36, !nosanitize !2, !afl_edge_sanitizer !2

handler.out_of_bounds:                            ; preds = %if.then5
  %20 = zext i32 %17 to i64, !dbg !62, !nosanitize !2
  call void @__ubsan_handle_out_of_bounds(i8* bitcast ({ { [13 x i8]*, i32, i32 }, { i16, i16, [13 x i8] }*, { i16, i16, [6 x i8] }* }* @5 to i8*), i64 %20) #5, !dbg !62, !nosanitize !2
  br label %cont6, !dbg !62, !nosanitize !2

cont6:                                            ; preds = %handler.out_of_bounds, %if.then5
  %idxprom = sext i32 %17 to i64, !dbg !64
  %arrayidx.offs = add i64 %idxprom, 0, !dbg !64
  %21 = add i64 0, %arrayidx.offs, !dbg !64
  %arrayidx = getelementptr inbounds [256 x i8]* %array, i32 0, i64 %idxprom, !dbg !64
  %22 = sub i64 256, %21, !dbg !64
  %23 = icmp ult i64 256, %21, !dbg !64
  %24 = icmp ult i64 %22, 1, !dbg !64
  %25 = or i1 %23, %24, !dbg !64
  br i1 %25, label %trap, label %26

; <label>:26                                      ; preds = %cont6
  store i8 %conv, i8* %arrayidx, align 1, !dbg !64
  br label %if.end, !dbg !67

if.else:                                          ; preds = %cont3
  %27 = load i32* %j, align 4, !dbg !68
  %28 = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 123, i32 %27), !dbg !70
  %29 = extractvalue { i32, i1 } %28, 0, !dbg !70
  %30 = extractvalue { i32, i1 } %28, 1, !dbg !70
  %31 = xor i1 %30, true, !dbg !70, !nosanitize !2
  br i1 %31, label %cont8, label %handler.add_overflow7, !dbg !70, !prof !36, !nosanitize !2, !afl_edge_sanitizer !2

handler.add_overflow7:                            ; preds = %if.else
  %32 = zext i32 %27 to i64, !dbg !71, !nosanitize !2
  call void @__ubsan_handle_add_overflow(i8* bitcast ({ { [13 x i8]*, i32, i32 }, { i16, i16, [6 x i8] }* }* @6 to i8*), i64 123, i64 %32) #5, !dbg !71, !nosanitize !2
  br label %cont8, !dbg !71, !nosanitize !2

cont8:                                            ; preds = %handler.add_overflow7, %if.else
  store i32 %29, i32* %i, align 4, !dbg !73
  br label %if.end

if.end:                                           ; preds = %cont8, %26
  br label %if.end9, !dbg !76

if.end9:                                          ; preds = %if.end, %cont1
  ret i32 0, !dbg !77

trap:                                             ; preds = %cont6
  call void @llvm.trap() #4, !dbg !64
  unreachable, !dbg !64
}

; Function Attrs: nounwind readnone
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

; Function Attrs: nounwind readnone
declare { i32, i1 } @llvm.sadd.with.overflow.i32(i32, i32) #1

; Function Attrs: uwtable
declare void @__ubsan_handle_add_overflow(i8*, i64, i64) #2

; Function Attrs: nounwind readnone
declare { i32, i1 } @llvm.ssub.with.overflow.i32(i32, i32) #1

; Function Attrs: uwtable
declare void @__ubsan_handle_negate_overflow(i8*, i64) #2

declare i32 @getchar() #3

; Function Attrs: uwtable
declare void @__ubsan_handle_out_of_bounds(i8*, i64) #2

; Function Attrs: noreturn nounwind
declare void @llvm.trap() #4

attributes #0 = { nounwind uwtable "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind readnone }
attributes #2 = { uwtable }
attributes #3 = { "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #4 = { noreturn nounwind }
attributes #5 = { nounwind }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!13, !14}
!llvm.ident = !{!15}

!0 = !{!"0x11\0012\00clang version 3.6.0 (tags/RELEASE_360/final)\000\00\000\00\001", !1, !2, !3, !5, !2, !2} ; [ DW_TAG_compile_unit ] [/home/eric/work/svf/test_bc/conflict-parent/test_float.c] [DW_LANG_C99]
!1 = !{!"test_float.c", !"/home/eric/work/svf/test_bc/conflict-parent"}
!2 = !{}
!3 = !{!4}
!4 = !{!"0x24\00int\000\0032\0032\000\000\005", null, null} ; [ DW_TAG_base_type ] [int] [line 0, size 32, align 32, offset 0, enc DW_ATE_signed]
!5 = !{!6}
!6 = !{!"0x2e\00main\00main\00\0010\000\001\000\000\00256\000\0010", !1, !7, !8, null, i32 (i32, i8**)* @main, null, null, !2} ; [ DW_TAG_subprogram ] [line 10] [def] [main]
!7 = !{!"0x29", !1}                               ; [ DW_TAG_file_type ] [/home/eric/work/svf/test_bc/conflict-parent/test_float.c]
!8 = !{!"0x15\00\000\000\000\000\000\000", null, null, null, !9, null, null, null} ; [ DW_TAG_subroutine_type ] [line 0, size 0, align 0, offset 0] [from ]
!9 = !{!4, !4, !10}
!10 = !{!"0xf\00\000\0064\0064\000\000", null, null, !11} ; [ DW_TAG_pointer_type ] [line 0, size 64, align 64, offset 0] [from ]
!11 = !{!"0xf\00\000\0064\0064\000\000", null, null, !12} ; [ DW_TAG_pointer_type ] [line 0, size 64, align 64, offset 0] [from char]
!12 = !{!"0x24\00char\000\008\008\000\000\006", null, null} ; [ DW_TAG_base_type ] [char] [line 0, size 8, align 8, offset 0, enc DW_ATE_signed_char]
!13 = !{i32 2, !"Dwarf Version", i32 4}
!14 = !{i32 2, !"Debug Info Version", i32 2}
!15 = !{!"clang version 3.6.0 (tags/RELEASE_360/final)"}
!16 = !{!"0x101\00argc\0016777226\000", !6, !7, !4} ; [ DW_TAG_arg_variable ] [argc] [line 10]
!17 = !{!"0x102"}                                 ; [ DW_TAG_expression ]
!18 = !MDLocation(line: 10, column: 14, scope: !6)
!19 = !{!"0x101\00argv\0033554442\000", !6, !7, !10} ; [ DW_TAG_arg_variable ] [argv] [line 10]
!20 = !MDLocation(line: 10, column: 26, scope: !6)
!21 = !{!"0x100\00array\0011\000", !6, !7, !22}   ; [ DW_TAG_auto_variable ] [array] [line 11]
!22 = !{!"0x1\00\000\002048\008\000\000\000", null, null, !12, !23, null, null, null} ; [ DW_TAG_array_type ] [line 0, size 2048, align 8, offset 0] [from char]
!23 = !{!24}
!24 = !{!"0x21\000\00256"}                        ; [ DW_TAG_subrange_type ] [0, 255]
!25 = !MDLocation(line: 11, column: 8, scope: !6)
!26 = !{!"0x100\00i\0012\000", !6, !7, !4}        ; [ DW_TAG_auto_variable ] [i] [line 12]
!27 = !MDLocation(line: 12, column: 7, scope: !6)
!28 = !{!"0x100\00z\0013\000", !6, !7, !29}       ; [ DW_TAG_auto_variable ] [z] [line 13]
!29 = !{!"0x24\00double\000\0064\0064\000\000\004", null, null} ; [ DW_TAG_base_type ] [double] [line 0, size 64, align 64, offset 0, enc DW_ATE_float]
!30 = !MDLocation(line: 13, column: 10, scope: !6)
!31 = !{!"0x100\00j\0014\000", !6, !7, !4}        ; [ DW_TAG_auto_variable ] [j] [line 14]
!32 = !MDLocation(line: 14, column: 7, scope: !6)
!33 = !MDLocation(line: 15, column: 7, scope: !34)
!34 = !{!"0xb\0015\007\000", !1, !6}              ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_float.c]
!35 = !MDLocation(line: 15, column: 11, scope: !34)
!36 = !{!"branch_weights", i32 1048575, i32 1}
!37 = !MDLocation(line: 15, column: 7, scope: !38)
!38 = !{!"0xb\002", !1, !34}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_float.c]
!39 = !MDLocation(line: 15, column: 15, scope: !40)
!40 = !{!"0xb\003", !1, !41}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_float.c]
!41 = !{!"0xb\001", !1, !34}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_float.c]
!42 = !MDLocation(line: 15, column: 15, scope: !43)
!43 = !{!"0xb\005", !1, !34}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_float.c]
!44 = !MDLocation(line: 15, column: 7, scope: !45)
!45 = !{!"0xb\006", !1, !46}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_float.c]
!46 = !{!"0xb\004", !1, !34}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_float.c]
!47 = !MDLocation(line: 15, column: 7, scope: !6)
!48 = !MDLocation(line: 16, column: 11, scope: !49)
!49 = !{!"0xb\0016\0011\002", !1, !50}            ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_float.c]
!50 = !{!"0xb\0015\0019\001", !1, !34}            ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_float.c]
!51 = !MDLocation(line: 16, column: 15, scope: !49)
!52 = !MDLocation(line: 16, column: 15, scope: !53)
!53 = !{!"0xb\002", !1, !49}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_float.c]
!54 = !MDLocation(line: 16, column: 11, scope: !55)
!55 = !{!"0xb\003", !1, !56}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_float.c]
!56 = !{!"0xb\001", !1, !49}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_float.c]
!57 = !MDLocation(line: 16, column: 11, scope: !50)
!58 = !MDLocation(line: 17, column: 27, scope: !59)
!59 = !{!"0xb\0016\0018\003", !1, !49}            ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_float.c]
!60 = !MDLocation(line: 17, column: 22, scope: !59)
!61 = !MDLocation(line: 17, column: 11, scope: !59)
!62 = !MDLocation(line: 17, column: 11, scope: !63)
!63 = !{!"0xb\002", !1, !59}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_float.c]
!64 = !MDLocation(line: 17, column: 11, scope: !65)
!65 = !{!"0xb\003", !1, !66}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_float.c]
!66 = !{!"0xb\001", !1, !59}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_float.c]
!67 = !MDLocation(line: 18, column: 7, scope: !59)
!68 = !MDLocation(line: 19, column: 26, scope: !69)
!69 = !{!"0xb\0018\0012\004", !1, !49}            ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_float.c]
!70 = !MDLocation(line: 19, column: 15, scope: !69)
!71 = !MDLocation(line: 19, column: 15, scope: !72)
!72 = !{!"0xb\002", !1, !69}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_float.c]
!73 = !MDLocation(line: 19, column: 11, scope: !74)
!74 = !{!"0xb\003", !1, !75}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_float.c]
!75 = !{!"0xb\001", !1, !69}                      ; [ DW_TAG_lexical_block ] [/home/eric/work/svf/test_bc/conflict-parent/test_float.c]
!76 = !MDLocation(line: 21, column: 3, scope: !50)
!77 = !MDLocation(line: 22, column: 3, scope: !6)
