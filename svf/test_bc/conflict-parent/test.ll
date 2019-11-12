; ModuleID = 'test.bc'
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.src = private unnamed_addr constant [7 x i8] c"test.c\00", align 1
@0 = private unnamed_addr constant { i16, i16, [13 x i8] } { i16 -1, i16 0, [13 x i8] c"'char [251]'\00" }
@1 = private unnamed_addr constant { i16, i16, [6 x i8] } { i16 0, i16 11, [6 x i8] c"'int'\00" }
@2 = private unnamed_addr global { { [7 x i8]*, i32, i32 }, { i16, i16, [13 x i8] }*, { i16, i16, [6 x i8] }* } { { [7 x i8]*, i32, i32 } { [7 x i8]* @.src, i32 8, i32 5 }, { i16, i16, [13 x i8] }* @0, { i16, i16, [6 x i8] }* @1 }
@3 = private unnamed_addr global { { [7 x i8]*, i32, i32 }, { i16, i16, [6 x i8] }* } { { [7 x i8]*, i32, i32 } { [7 x i8]* @.src, i32 9, i32 6 }, { i16, i16, [6 x i8] }* @1 }

; Function Attrs: nounwind uwtable
define i32 @main(i32 %argc, i8** %argv) #0 {
entry:
  %retval = alloca i32, align 4
  %argc.addr = alloca i32, align 4
  %argv.addr = alloca i8**, align 8
  %array = alloca [251 x i8], align 16
  %i = alloca i32, align 4
  store i32 0, i32* %retval
  store i32 %argc, i32* %argc.addr, align 4
  store i8** %argv, i8*** %argv.addr, align 8
  store i32 0, i32* %i, align 4
  br label %for.cond

for.cond:                                         ; preds = %cont1, %entry
  %0 = load i32* %i, align 4
  %cmp = icmp slt i32 %0, 256
  br i1 %cmp, label %for.body, label %for.end

for.body:                                         ; preds = %for.cond
  %call = call i32 @getchar()
  %conv = trunc i32 %call to i8
  %1 = load i32* %i, align 4
  %2 = sext i32 %1 to i64, !nosanitize !1
  %3 = icmp ult i64 %2, 251, !nosanitize !1
  br i1 %3, label %cont, label %handler.out_of_bounds, !prof !2, !nosanitize !1, !afl_edge_sanitizer !1

handler.out_of_bounds:                            ; preds = %for.body
  %4 = zext i32 %1 to i64, !nosanitize !1
  call void @__ubsan_handle_out_of_bounds(i8* bitcast ({ { [7 x i8]*, i32, i32 }, { i16, i16, [13 x i8] }*, { i16, i16, [6 x i8] }* }* @2 to i8*), i64 %4) #5, !nosanitize !1
  br label %cont, !nosanitize !1

cont:                                             ; preds = %handler.out_of_bounds, %for.body
  %idxprom = sext i32 %1 to i64
  %arrayidx.offs = add i64 %idxprom, 0
  %5 = add i64 0, %arrayidx.offs
  %arrayidx = getelementptr inbounds [251 x i8]* %array, i32 0, i64 %idxprom
  %6 = sub i64 256, %5
  %7 = icmp ult i64 256, %5
  %8 = icmp ult i64 %6, 1
  %9 = or i1 %7, %8
  br i1 %9, label %trap, label %10

; <label>:10                                      ; preds = %cont
  store i8 %conv, i8* %arrayidx, align 1
  %11 = load i32* %i, align 4
  %12 = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %11, i32 1)
  %13 = extractvalue { i32, i1 } %12, 0
  %14 = extractvalue { i32, i1 } %12, 1
  %15 = xor i1 %14, true, !nosanitize !1
  br i1 %15, label %cont1, label %handler.add_overflow, !prof !2, !nosanitize !1, !afl_edge_sanitizer !1

handler.add_overflow:                             ; preds = %10
  %16 = zext i32 %11 to i64, !nosanitize !1
  call void @__ubsan_handle_add_overflow(i8* bitcast ({ { [7 x i8]*, i32, i32 }, { i16, i16, [6 x i8] }* }* @3 to i8*), i64 %16, i64 1) #5, !nosanitize !1
  br label %cont1, !nosanitize !1

cont1:                                            ; preds = %handler.add_overflow, %10
  store i32 %13, i32* %i, align 4
  br label %for.cond

for.end:                                          ; preds = %for.cond
  ret i32 0

trap:                                             ; preds = %cont
  call void @llvm.trap() #4
  unreachable
}

declare i32 @getchar() #1

; Function Attrs: uwtable
declare void @__ubsan_handle_out_of_bounds(i8*, i64) #2

; Function Attrs: nounwind readnone
declare { i32, i1 } @llvm.sadd.with.overflow.i32(i32, i32) #3

; Function Attrs: uwtable
declare void @__ubsan_handle_add_overflow(i8*, i64, i64) #2

; Function Attrs: noreturn nounwind
declare void @llvm.trap() #4

attributes #0 = { nounwind uwtable "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { uwtable }
attributes #3 = { nounwind readnone }
attributes #4 = { noreturn nounwind }
attributes #5 = { nounwind }

!llvm.ident = !{!0}

!0 = !{!"clang version 3.6.0 (tags/RELEASE_360/final)"}
!1 = !{}
!2 = !{!"branch_weights", i32 1048575, i32 1}
