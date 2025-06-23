#ifndef s_p_ctx_name
#error "parameter s_p_ctx_name not defined"
#endif

    call s_p_ctx_name%exit_parallel()
end block

#undef s_p_ctx_name