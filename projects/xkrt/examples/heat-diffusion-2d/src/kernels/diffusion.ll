; ModuleID = 'diffusion.cl'
source_filename = "diffusion.cl"
target datalayout = "e-p:32:32-i64:64-v16:16-v24:32-v32:32-v48:64-v96:128-v192:256-v256:256-v512:512-v1024:1024-G1"
target triple = "spir-unknown-unknown"

; Function Attrs: convergent mustprogress nofree norecurse nounwind willreturn memory(argmem: readwrite)
define dso_local spir_kernel void @diffusion_cl_kernel(ptr addrspace(1) noundef readonly align 4 captures(none) %src, i32 noundef %ld_src, ptr addrspace(1) noundef writeonly align 4 captures(none) %dst, i32 noundef %ld_dst, i32 noundef %tile_x, i32 noundef %tile_y, i32 noundef %tsx, i32 noundef %tsy) local_unnamed_addr #0 !kernel_arg_addr_space !3 !kernel_arg_access_qual !4 !kernel_arg_type !5 !kernel_arg_base_type !5 !kernel_arg_type_qual !6 {
entry:
  %call = tail call spir_func i32 @_Z13get_global_idj(i32 noundef 0) #3
  %call1 = tail call spir_func i32 @_Z13get_global_idj(i32 noundef 1) #3
  %mul = mul nsw i32 %tsx, %tile_x
  %mul2 = mul nsw i32 %tsy, %tile_y
  %add = add i32 %mul, -1
  %0 = add i32 %add, %call
  %or.cond = icmp ult i32 %0, 14
  %add3 = add i32 %call1, -1
  %1 = add i32 %add3, %mul2
  %2 = icmp ult i32 %1, 14
  %or.cond46 = select i1 %or.cond, i1 %2, i1 false
  br i1 %or.cond46, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %mul9 = mul nsw i32 %call1, %ld_src
  %3 = getelementptr float, ptr addrspace(1) %src, i32 %mul9
  %arrayidx = getelementptr float, ptr addrspace(1) %3, i32 %call
  %4 = load float, ptr addrspace(1) %arrayidx, align 4, !tbaa !7
  %conv = fpext float %4 to double
  %arrayidx14 = getelementptr i8, ptr addrspace(1) %arrayidx, i32 4
  %5 = load float, ptr addrspace(1) %arrayidx14, align 4, !tbaa !7
  %6 = tail call float @llvm.fmuladd.f32(float %4, float -2.000000e+00, float %5)
  %arrayidx21 = getelementptr i8, ptr addrspace(1) %arrayidx, i32 -4
  %7 = load float, ptr addrspace(1) %arrayidx21, align 4, !tbaa !7
  %add22 = fadd float %6, %7
  %conv23 = fpext float %add22 to double
  %add24 = add nsw i32 %call1, 1
  %mul25 = mul nsw i32 %add24, %ld_src
  %8 = getelementptr float, ptr addrspace(1) %src, i32 %mul25
  %arrayidx27 = getelementptr float, ptr addrspace(1) %8, i32 %call
  %9 = load float, ptr addrspace(1) %arrayidx27, align 4, !tbaa !7
  %10 = tail call float @llvm.fmuladd.f32(float %4, float -2.000000e+00, float %9)
  %mul33 = mul nsw i32 %add3, %ld_src
  %11 = getelementptr float, ptr addrspace(1) %src, i32 %mul33
  %arrayidx35 = getelementptr float, ptr addrspace(1) %11, i32 %call
  %12 = load float, ptr addrspace(1) %arrayidx35, align 4, !tbaa !7
  %add36 = fadd float %10, %12
  %conv37 = fpext float %add36 to double
  %add39 = fadd double %conv23, %conv37
  %13 = tail call double @llvm.fmuladd.f64(double %add39, double 1.250000e-01, double %conv)
  %conv41 = fptrunc double %13 to float
  %mul42 = mul nsw i32 %call1, %ld_dst
  %14 = getelementptr float, ptr addrspace(1) %dst, i32 %mul42
  %arrayidx44 = getelementptr float, ptr addrspace(1) %14, i32 %call
  store float %conv41, ptr addrspace(1) %arrayidx44, align 4, !tbaa !7
  br label %if.end

if.end:                                           ; preds = %if.then, %entry
  ret void
}

; Function Attrs: convergent mustprogress nofree nounwind willreturn memory(none)
declare spir_func i32 @_Z13get_global_idj(i32 noundef) local_unnamed_addr #1

; Function Attrs: mustprogress nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare float @llvm.fmuladd.f32(float, float, float) #2

; Function Attrs: mustprogress nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare double @llvm.fmuladd.f64(double, double, double) #2

attributes #0 = { convergent mustprogress nofree norecurse nounwind willreturn memory(argmem: readwrite) "no-trapping-math"="true" "stack-protector-buffer-size"="8" "uniform-work-group-size"="false" }
attributes #1 = { convergent mustprogress nofree nounwind willreturn memory(none) "no-trapping-math"="true" "stack-protector-buffer-size"="8" }
attributes #2 = { mustprogress nocallback nofree nosync nounwind speculatable willreturn memory(none) }
attributes #3 = { convergent nounwind willreturn memory(none) }

!llvm.module.flags = !{!0}
!opencl.ocl.version = !{!1}
!opencl.spir.version = !{!1}
!llvm.ident = !{!2}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 3, i32 0}
!2 = !{!"clang version 21.0.0git (https://github.com/llvm/llvm-project.git cd4c30bb224e432d8cd37f375c138cbaada14f6c)"}
!3 = !{i32 1, i32 0, i32 1, i32 0, i32 0, i32 0, i32 0, i32 0}
!4 = !{!"none", !"none", !"none", !"none", !"none", !"none", !"none", !"none"}
!5 = !{!"float*", !"int", !"float*", !"int", !"int", !"int", !"int", !"int"}
!6 = !{!"", !"", !"", !"", !"", !"", !"", !""}
!7 = !{!8, !8, i64 0}
!8 = !{!"float", !9, i64 0}
!9 = !{!"omnipotent char", !10, i64 0}
!10 = !{!"Simple C/C++ TBAA"}
