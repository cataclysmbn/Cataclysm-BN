// Placeholder compute shader for shader toolchain validation.
// Does nothing — exists to exercise the full HLSL → blob compile pipeline.
[numthreads(1, 1, 1)]
void main( uint3 dispatch_id : SV_DispatchThreadID ) {}
