#ifndef s_p_enter_parallel
#error "parameter s_p_enter_parallel not defined"
#endif

#ifndef s_p_ctx_name
#error "parameter s_p_ctx_name not defined"
#endif

block
   use::mpi_f08
   use::simple_parallel
   use::omp_lib

   type(par_ctx)::s_p_ctx_name

   call s_p_ctx_name%init()

#ifdef s_p_reduce_1
   call s_p_ctx_name%add_reduce(s_p_reduce_1)
#endif

#ifdef s_p_reduce_2
   call s_p_ctx_name%add_reduce(s_p_reduce_2)
#endif

#ifdef s_p_reduce_3
   call s_p_ctx_name%add_reduce(s_p_reduce_3)
#endif

#ifdef s_p_reduce_4
   call s_p_ctx_name%add_reduce(s_p_reduce_4)
#endif

#ifdef s_p_reduce_5
   call s_p_ctx_name%add_reduce(s_p_reduce_5)
#endif

#ifdef s_p_reduce_6
   call s_p_ctx_name%add_reduce(s_p_reduce_6)
#endif

#ifdef s_p_reduce_7
   call s_p_ctx_name%add_reduce(s_p_reduce_7)
#endif

#ifdef s_p_reduce_8
   call s_p_ctx_name%add_reduce(s_p_reduce_8)
#endif

#ifdef s_p_reduce_9
   call s_p_ctx_name%add_reduce(s_p_reduce_9)
#endif

#ifdef s_p_reduce_10
   call s_p_ctx_name%add_reduce(s_p_reduce_10)
#endif

#ifdef s_p_reduce_11
   call s_p_ctx_name%add_reduce(s_p_reduce_11)
#endif

#ifdef s_p_reduce_12
   call s_p_ctx_name%add_reduce(s_p_reduce_12)
#endif

#ifdef s_p_reduce_13
   call s_p_ctx_name%add_reduce(s_p_reduce_13)
#endif

#ifdef s_p_reduce_14
   call s_p_ctx_name%add_reduce(s_p_reduce_14)
#endif

#ifdef s_p_reduce_15
   call s_p_ctx_name%add_reduce(s_p_reduce_15)
#endif

#ifdef s_p_reduce_16
   call s_p_ctx_name%add_reduce(s_p_reduce_16)
#endif

#ifdef s_p_reduce_17
   call s_p_ctx_name%add_reduce(s_p_reduce_17)
#endif

#ifdef s_p_reduce_18
   call s_p_ctx_name%add_reduce(s_p_reduce_18)
#endif

#ifdef s_p_reduce_19
   call s_p_ctx_name%add_reduce(s_p_reduce_19)
#endif

#ifdef s_p_reduce_20
   call s_p_ctx_name%add_reduce(s_p_reduce_20)
#endif

#ifdef s_p_reduce_21
   call s_p_ctx_name%add_reduce(s_p_reduce_21)
#endif

#ifdef s_p_reduce_22
   call s_p_ctx_name%add_reduce(s_p_reduce_22)
#endif

#ifdef s_p_reduce_23
   call s_p_ctx_name%add_reduce(s_p_reduce_23)
#endif

#ifdef s_p_reduce_24
   call s_p_ctx_name%add_reduce(s_p_reduce_24)
#endif

#ifdef s_p_reduce_25
   call s_p_ctx_name%add_reduce(s_p_reduce_25)
#endif

#ifdef s_p_reduce_26
   call s_p_ctx_name%add_reduce(s_p_reduce_26)
#endif

#ifdef s_p_reduce_27
   call s_p_ctx_name%add_reduce(s_p_reduce_27)
#endif

#ifdef s_p_reduce_28
   call s_p_ctx_name%add_reduce(s_p_reduce_28)
#endif

#ifdef s_p_reduce_29
   call s_p_ctx_name%add_reduce(s_p_reduce_29)
#endif

#ifdef s_p_reduce_30
   call s_p_ctx_name%add_reduce(s_p_reduce_30)
#endif

#ifdef s_p_reduce_31
   call s_p_ctx_name%add_reduce(s_p_reduce_31)
#endif

#ifdef s_p_reduce_32
   call s_p_ctx_name%add_reduce(s_p_reduce_32)
#endif

#ifdef s_p_reduce_33
   call s_p_ctx_name%add_reduce(s_p_reduce_33)
#endif

#ifdef s_p_reduce_34
   call s_p_ctx_name%add_reduce(s_p_reduce_34)
#endif

#ifdef s_p_reduce_35
   call s_p_ctx_name%add_reduce(s_p_reduce_35)
#endif

#ifdef s_p_reduce_36
   call s_p_ctx_name%add_reduce(s_p_reduce_36)
#endif

#ifdef s_p_reduce_37
   call s_p_ctx_name%add_reduce(s_p_reduce_37)
#endif

#ifdef s_p_reduce_38
   call s_p_ctx_name%add_reduce(s_p_reduce_38)
#endif

#ifdef s_p_reduce_39
   call s_p_ctx_name%add_reduce(s_p_reduce_39)
#endif

#ifdef s_p_reduce_40
   call s_p_ctx_name%add_reduce(s_p_reduce_40)
#endif

#ifdef s_p_reduce_41
   call s_p_ctx_name%add_reduce(s_p_reduce_41)
#endif

#ifdef s_p_reduce_42
   call s_p_ctx_name%add_reduce(s_p_reduce_42)
#endif

#ifdef s_p_reduce_43
   call s_p_ctx_name%add_reduce(s_p_reduce_43)
#endif

#ifdef s_p_reduce_44
   call s_p_ctx_name%add_reduce(s_p_reduce_44)
#endif

#ifdef s_p_reduce_45
   call s_p_ctx_name%add_reduce(s_p_reduce_45)
#endif

#ifdef s_p_reduce_46
   call s_p_ctx_name%add_reduce(s_p_reduce_46)
#endif

#ifdef s_p_reduce_47
   call s_p_ctx_name%add_reduce(s_p_reduce_47)
#endif

#ifdef s_p_reduce_48
   call s_p_ctx_name%add_reduce(s_p_reduce_48)
#endif

#ifdef s_p_reduce_49
   call s_p_ctx_name%add_reduce(s_p_reduce_49)
#endif

#ifdef s_p_reduce_50
   call s_p_ctx_name%add_reduce(s_p_reduce_50)
#endif

#ifdef s_p_reduce_51
   call s_p_ctx_name%add_reduce(s_p_reduce_51)
#endif

#ifdef s_p_reduce_52
   call s_p_ctx_name%add_reduce(s_p_reduce_52)
#endif

#ifdef s_p_reduce_53
   call s_p_ctx_name%add_reduce(s_p_reduce_53)
#endif

#ifdef s_p_reduce_54
   call s_p_ctx_name%add_reduce(s_p_reduce_54)
#endif

#ifdef s_p_reduce_55
   call s_p_ctx_name%add_reduce(s_p_reduce_55)
#endif

#ifdef s_p_reduce_56
   call s_p_ctx_name%add_reduce(s_p_reduce_56)
#endif

#ifdef s_p_reduce_57
   call s_p_ctx_name%add_reduce(s_p_reduce_57)
#endif

#ifdef s_p_reduce_58
   call s_p_ctx_name%add_reduce(s_p_reduce_58)
#endif

#ifdef s_p_reduce_59
   call s_p_ctx_name%add_reduce(s_p_reduce_59)
#endif

#ifdef s_p_reduce_60
   call s_p_ctx_name%add_reduce(s_p_reduce_60)
#endif

#ifdef s_p_reduce_61
   call s_p_ctx_name%add_reduce(s_p_reduce_61)
#endif

#ifdef s_p_reduce_62
   call s_p_ctx_name%add_reduce(s_p_reduce_62)
#endif

#ifdef s_p_reduce_63
   call s_p_ctx_name%add_reduce(s_p_reduce_63)
#endif

#ifdef s_p_reduce_64
   call s_p_ctx_name%add_reduce(s_p_reduce_64)
#endif

   if (s_p_enter_parallel) then
      call s_p_ctx_name%enter_parallel()
   endif

#undef s_p_enter_parallel
! s_p_ctx_name is undefed in parallel_end.h
! #undef s_p_ctx_name

#ifdef s_p_reduce_1
#undef s_p_reduce_1
#endif
#ifdef s_p_reduce_2
#undef s_p_reduce_2
#endif
#ifdef s_p_reduce_3
#undef s_p_reduce_3
#endif
#ifdef s_p_reduce_4
#undef s_p_reduce_4
#endif
#ifdef s_p_reduce_5
#undef s_p_reduce_5
#endif
#ifdef s_p_reduce_6
#undef s_p_reduce_6
#endif
#ifdef s_p_reduce_7
#undef s_p_reduce_7
#endif
#ifdef s_p_reduce_8
#undef s_p_reduce_8
#endif
#ifdef s_p_reduce_9
#undef s_p_reduce_9
#endif
#ifdef s_p_reduce_10
#undef s_p_reduce_10
#endif
#ifdef s_p_reduce_11
#undef s_p_reduce_11
#endif
#ifdef s_p_reduce_12
#undef s_p_reduce_12
#endif
#ifdef s_p_reduce_13
#undef s_p_reduce_13
#endif
#ifdef s_p_reduce_14
#undef s_p_reduce_14
#endif
#ifdef s_p_reduce_15
#undef s_p_reduce_15
#endif
#ifdef s_p_reduce_16
#undef s_p_reduce_16
#endif
#ifdef s_p_reduce_17
#undef s_p_reduce_17
#endif
#ifdef s_p_reduce_18
#undef s_p_reduce_18
#endif
#ifdef s_p_reduce_19
#undef s_p_reduce_19
#endif
#ifdef s_p_reduce_20
#undef s_p_reduce_20
#endif
#ifdef s_p_reduce_21
#undef s_p_reduce_21
#endif
#ifdef s_p_reduce_22
#undef s_p_reduce_22
#endif
#ifdef s_p_reduce_23
#undef s_p_reduce_23
#endif
#ifdef s_p_reduce_24
#undef s_p_reduce_24
#endif
#ifdef s_p_reduce_25
#undef s_p_reduce_25
#endif
#ifdef s_p_reduce_26
#undef s_p_reduce_26
#endif
#ifdef s_p_reduce_27
#undef s_p_reduce_27
#endif
#ifdef s_p_reduce_28
#undef s_p_reduce_28
#endif
#ifdef s_p_reduce_29
#undef s_p_reduce_29
#endif
#ifdef s_p_reduce_30
#undef s_p_reduce_30
#endif
#ifdef s_p_reduce_31
#undef s_p_reduce_31
#endif
#ifdef s_p_reduce_32
#undef s_p_reduce_32
#endif
#ifdef s_p_reduce_33
#undef s_p_reduce_33
#endif
#ifdef s_p_reduce_34
#undef s_p_reduce_34
#endif
#ifdef s_p_reduce_35
#undef s_p_reduce_35
#endif
#ifdef s_p_reduce_36
#undef s_p_reduce_36
#endif
#ifdef s_p_reduce_37
#undef s_p_reduce_37
#endif
#ifdef s_p_reduce_38
#undef s_p_reduce_38
#endif
#ifdef s_p_reduce_39
#undef s_p_reduce_39
#endif
#ifdef s_p_reduce_40
#undef s_p_reduce_40
#endif
#ifdef s_p_reduce_41
#undef s_p_reduce_41
#endif
#ifdef s_p_reduce_42
#undef s_p_reduce_42
#endif
#ifdef s_p_reduce_43
#undef s_p_reduce_43
#endif
#ifdef s_p_reduce_44
#undef s_p_reduce_44
#endif
#ifdef s_p_reduce_45
#undef s_p_reduce_45
#endif
#ifdef s_p_reduce_46
#undef s_p_reduce_46
#endif
#ifdef s_p_reduce_47
#undef s_p_reduce_47
#endif
#ifdef s_p_reduce_48
#undef s_p_reduce_48
#endif
#ifdef s_p_reduce_49
#undef s_p_reduce_49
#endif
#ifdef s_p_reduce_50
#undef s_p_reduce_50
#endif
#ifdef s_p_reduce_51
#undef s_p_reduce_51
#endif
#ifdef s_p_reduce_52
#undef s_p_reduce_52
#endif
#ifdef s_p_reduce_53
#undef s_p_reduce_53
#endif
#ifdef s_p_reduce_54
#undef s_p_reduce_54
#endif
#ifdef s_p_reduce_55
#undef s_p_reduce_55
#endif
#ifdef s_p_reduce_56
#undef s_p_reduce_56
#endif
#ifdef s_p_reduce_57
#undef s_p_reduce_57
#endif
#ifdef s_p_reduce_58
#undef s_p_reduce_58
#endif
#ifdef s_p_reduce_59
#undef s_p_reduce_59
#endif
#ifdef s_p_reduce_60
#undef s_p_reduce_60
#endif
#ifdef s_p_reduce_61
#undef s_p_reduce_61
#endif
#ifdef s_p_reduce_62
#undef s_p_reduce_62
#endif
#ifdef s_p_reduce_63
#undef s_p_reduce_63
#endif
#ifdef s_p_reduce_64
#undef s_p_reduce_64
#endif