0000000000010820 <intersect_batch(SceneData const&, Ray const*, Hit*)>:
   10820:	48 81 ec 08 01 00 00 	sub    $0x108,%rsp
   10827:	c5 f8 57 c0          	vxorps %xmm0,%xmm0,%xmm0
   1082b:	c5 fc 11 44 24 a0    	vmovups %ymm0,-0x60(%rsp)
   10831:	c5 fc 11 44 24 80    	vmovups %ymm0,-0x80(%rsp)
   10837:	48 63 87 00 12 00 00 	movslq 0x1200(%rdi),%rax
   1083e:	48 85 c0             	test   %rax,%rax
   10841:	0f 8e a9 03 00 00    	jle    10bf0 <intersect_batch(SceneData const&, Ray const*, Hit*)+0x3d0>
   10847:	c5 fa 10 4e 60       	vmovss 0x60(%rsi),%xmm1
   1084c:	c4 e3 71 21 4e 78 10 	vinsertps $0x10,0x78(%rsi),%xmm1,%xmm1
   10853:	c4 e3 71 21 8e 90 00 	vinsertps $0x20,0x90(%rsi),%xmm1,%xmm1
   1085a:	00 00 20
   1085d:	c4 e3 71 21 8e a8 00 	vinsertps $0x30,0xa8(%rsi),%xmm1,%xmm1
   10864:	00 00 30
   10867:	c5 fa 10 16          	vmovss (%rsi),%xmm2
   1086b:	c4 e3 69 21 56 18 10 	vinsertps $0x10,0x18(%rsi),%xmm2,%xmm2
   10872:	c4 e3 69 21 56 30 20 	vinsertps $0x20,0x30(%rsi),%xmm2,%xmm2
   10879:	c4 e3 69 21 56 48 30 	vinsertps $0x30,0x48(%rsi),%xmm2,%xmm2
   10880:	c5 fa 10 5e 04       	vmovss 0x4(%rsi),%xmm3
   10885:	c5 fa 10 66 64       	vmovss 0x64(%rsi),%xmm4
   1088a:	c4 e3 59 21 66 7c 10 	vinsertps $0x10,0x7c(%rsi),%xmm4,%xmm4
   10891:	c4 e3 59 21 a6 94 00 	vinsertps $0x20,0x94(%rsi),%xmm4,%xmm4
   10898:	00 00 20
   1089b:	c4 e3 59 21 a6 ac 00 	vinsertps $0x30,0xac(%rsi),%xmm4,%xmm4
   108a2:	00 00 30
   108a5:	c4 e3 61 21 5e 1c 10 	vinsertps $0x10,0x1c(%rsi),%xmm3,%xmm3
   108ac:	c4 e3 61 21 5e 34 20 	vinsertps $0x20,0x34(%rsi),%xmm3,%xmm3
   108b3:	c5 fa 10 6e 08       	vmovss 0x8(%rsi),%xmm5
   108b8:	c4 e3 61 21 76 4c 30 	vinsertps $0x30,0x4c(%rsi),%xmm3,%xmm6
   108bf:	c5 fa 10 5e 68       	vmovss 0x68(%rsi),%xmm3
   108c4:	c4 e3 61 21 9e 80 00 	vinsertps $0x10,0x80(%rsi),%xmm3,%xmm3
   108cb:	00 00 10
   108ce:	c4 e3 61 21 9e 98 00 	vinsertps $0x20,0x98(%rsi),%xmm3,%xmm3
   108d5:	00 00 20
   108d8:	c4 e3 61 21 be b0 00 	vinsertps $0x30,0xb0(%rsi),%xmm3,%xmm7
   108df:	00 00 30
   108e2:	c4 e3 51 21 5e 20 10 	vinsertps $0x10,0x20(%rsi),%xmm5,%xmm3
   108e9:	c5 fa 10 6e 0c       	vmovss 0xc(%rsi),%xmm5
   108ee:	c4 e3 61 21 5e 38 20 	vinsertps $0x20,0x38(%rsi),%xmm3,%xmm3
   108f5:	c4 63 61 21 46 50 30 	vinsertps $0x30,0x50(%rsi),%xmm3,%xmm8
   108fc:	c5 fa 10 5e 6c       	vmovss 0x6c(%rsi),%xmm3
   10901:	c4 e3 61 21 9e 84 00 	vinsertps $0x10,0x84(%rsi),%xmm3,%xmm3
   10908:	00 00 10
   1090b:	c4 e3 61 21 9e 9c 00 	vinsertps $0x20,0x9c(%rsi),%xmm3,%xmm3
   10912:	00 00 20
   10915:	c4 63 61 21 8e b4 00 	vinsertps $0x30,0xb4(%rsi),%xmm3,%xmm9
   1091c:	00 00 30
   1091f:	c4 e3 51 21 5e 24 10 	vinsertps $0x10,0x24(%rsi),%xmm5,%xmm3
   10926:	c4 e3 61 21 5e 3c 20 	vinsertps $0x20,0x3c(%rsi),%xmm3,%xmm3
   1092d:	c4 63 61 21 56 54 30 	vinsertps $0x30,0x54(%rsi),%xmm3,%xmm10
   10934:	c4 e3 6d 18 c1 01    	vinsertf128 $0x1,%xmm1,%ymm2,%ymm0
   1093a:	c5 fc 11 84 24 80 00 	vmovups %ymm0,0x80(%rsp)
   10941:	00 00
   10943:	c5 fa 10 4e 70       	vmovss 0x70(%rsi),%xmm1
   10948:	c4 e3 71 21 8e 88 00 	vinsertps $0x10,0x88(%rsi),%xmm1,%xmm1
   1094f:	00 00 10
   10952:	c4 e3 71 21 8e a0 00 	vinsertps $0x20,0xa0(%rsi),%xmm1,%xmm1
   10959:	00 00 20
   1095c:	c4 e3 4d 18 c4 01    	vinsertf128 $0x1,%xmm4,%ymm6,%ymm0
   10962:	c5 fc 11 44 24 60    	vmovups %ymm0,0x60(%rsp)
   10968:	c4 e3 71 21 8e b8 00 	vinsertps $0x30,0xb8(%rsi),%xmm1,%xmm1
   1096f:	00 00 30
   10972:	c5 fa 10 56 10       	vmovss 0x10(%rsi),%xmm2
   10977:	c4 e3 69 21 56 28 10 	vinsertps $0x10,0x28(%rsi),%xmm2,%xmm2
   1097e:	c4 e3 3d 18 c7 01    	vinsertf128 $0x1,%xmm7,%ymm8,%ymm0
   10984:	c5 fc 11 44 24 40    	vmovups %ymm0,0x40(%rsp)
   1098a:	c4 e3 69 21 56 40 20 	vinsertps $0x20,0x40(%rsi),%xmm2,%xmm2
   10991:	c4 e3 69 21 56 58 30 	vinsertps $0x30,0x58(%rsi),%xmm2,%xmm2
   10998:	c5 fa 10 76 74       	vmovss 0x74(%rsi),%xmm6
   1099d:	c4 e3 49 21 b6 8c 00 	vinsertps $0x10,0x8c(%rsi),%xmm6,%xmm6
   109a4:	00 00 10
   109a7:	c4 e3 49 21 b6 a4 00 	vinsertps $0x20,0xa4(%rsi),%xmm6,%xmm6
   109ae:	00 00 20
   109b1:	c4 63 49 21 86 bc 00 	vinsertps $0x30,0xbc(%rsi),%xmm6,%xmm8
   109b8:	00 00 30
   109bb:	c4 c3 2d 18 c1 01    	vinsertf128 $0x1,%xmm9,%ymm10,%ymm0
   109c1:	c5 fc 11 44 24 20    	vmovups %ymm0,0x20(%rsp)
   109c7:	c5 fa 10 7e 14       	vmovss 0x14(%rsi),%xmm7
   109cc:	c4 e3 41 21 7e 2c 10 	vinsertps $0x10,0x2c(%rsi),%xmm7,%xmm7
   109d3:	c4 63 41 21 4e 44 20 	vinsertps $0x20,0x44(%rsi),%xmm7,%xmm9
   109da:	c4 e3 6d 18 c1 01    	vinsertf128 $0x1,%xmm1,%ymm2,%ymm0
   109e0:	c5 fc 11 04 24       	vmovups %ymm0,(%rsp)
   109e5:	c4 e3 31 21 4e 5c 30 	vinsertps $0x30,0x5c(%rsi),%xmm9,%xmm1
   109ec:	c4 c3 75 18 c0 01    	vinsertf128 $0x1,%xmm8,%ymm1,%ymm0
   109f2:	c5 fc 11 44 24 e0    	vmovups %ymm0,-0x20(%rsp)
   109f8:	48 83 c7 20          	add    $0x20,%rdi
   109fc:	c5 ed 76 d2          	vpcmpeqd %ymm2,%ymm2,%ymm2
   10a00:	c4 e2 7d 18 0d d3 46 	vbroadcastss 0x46d3(%rip),%ymm1        # 150dc <_IO_stdin_used+0xdc>
   10a07:	00 00
   10a09:	31 c9                	xor    %ecx,%ecx
   10a0b:	c4 e2 7d 18 05 04 46 	vbroadcastss 0x4604(%rip),%ymm0        # 15018 <_IO_stdin_used+0x18>
   10a12:	00 00
   10a14:	c5 fc 11 44 24 c0    	vmovups %ymm0,-0x40(%rsp)
   10a1a:	c5 7c 10 6c 24 e0    	vmovups -0x20(%rsp),%ymm13
   10a20:	c5 fc 11 8c 24 c0 00 	vmovups %ymm1,0xc0(%rsp)
   10a27:	00 00
   10a29:	c5 fe 7f 94 24 e0 00 	vmovdqu %ymm2,0xe0(%rsp)
   10a30:	00 00
   10a32:	c4 e2 7d 18 27       	vbroadcastss (%rdi),%ymm4
   10a37:	c5 fc 11 a4 24 a0 00 	vmovups %ymm4,0xa0(%rsp)
   10a3e:	00 00
   10a40:	c5 fb 10 4f e0       	vmovsd -0x20(%rdi),%xmm1
   10a45:	c5 7b 10 7f ec       	vmovsd -0x14(%rdi),%xmm15
   10a4a:	c5 7b 10 67 f8       	vmovsd -0x8(%rdi),%xmm12
   10a4f:	c5 fc 10 44 24 c0    	vmovups -0x40(%rsp),%ymm0
   10a55:	c4 42 7d 16 f4       	vpermps %ymm12,%ymm0,%ymm14
   10a5a:	c4 c1 14 59 de       	vmulps %ymm14,%ymm13,%ymm3
   10a5f:	c5 fc 10 14 24       	vmovups (%rsp),%ymm2
   10a64:	c4 e2 6d ba dc       	vfmsub231ps %ymm4,%ymm2,%ymm3
   10a69:	c4 42 7d 18 e4       	vbroadcastss %xmm12,%ymm12
   10a6e:	c5 7c 10 4c 24 20    	vmovups 0x20(%rsp),%ymm9
   10a74:	c5 b4 59 e4          	vmulps %ymm4,%ymm9,%ymm4
   10a78:	c4 c2 15 ba e4       	vfmsub231ps %ymm12,%ymm13,%ymm4
   10a7d:	c5 9c 59 ea          	vmulps %ymm2,%ymm12,%ymm5
   10a81:	c4 c2 35 ba ee       	vfmsub231ps %ymm14,%ymm9,%ymm5
   10a86:	c4 42 7d 18 d7       	vbroadcastss %xmm15,%ymm10
   10a8b:	c4 42 7d 16 df       	vpermps %ymm15,%ymm0,%ymm11
   10a90:	c5 24 59 fc          	vmulps %ymm4,%ymm11,%ymm15
   10a94:	c4 62 2d b8 fb       	vfmadd231ps %ymm3,%ymm10,%ymm15
   10a99:	c4 e2 7d 18 77 f4    	vbroadcastss -0xc(%rdi),%ymm6
   10a9f:	c4 62 4d b8 fd       	vfmadd231ps %ymm5,%ymm6,%ymm15
   10aa4:	c4 e2 7d 18 f9       	vbroadcastss %xmm1,%ymm7
   10aa9:	c5 7c 10 84 24 80 00 	vmovups 0x80(%rsp),%ymm8
   10ab0:	00 00
   10ab2:	c5 bc 5c ff          	vsubps %ymm7,%ymm8,%ymm7
   10ab6:	c4 e2 7d 16 c1       	vpermps %ymm1,%ymm0,%ymm0
   10abb:	c5 7c 10 44 24 60    	vmovups 0x60(%rsp),%ymm8
   10ac1:	c5 bc 5c c0          	vsubps %ymm0,%ymm8,%ymm0
   10ac5:	c5 dc 59 e0          	vmulps %ymm0,%ymm4,%ymm4
   10ac9:	c4 e2 45 b8 e3       	vfmadd231ps %ymm3,%ymm7,%ymm4
   10ace:	c4 e2 7d 18 5f e8    	vbroadcastss -0x18(%rdi),%ymm3
   10ad4:	c5 7c 10 44 24 40    	vmovups 0x40(%rsp),%ymm8
   10ada:	c5 bc 5c db          	vsubps %ymm3,%ymm8,%ymm3
   10ade:	c4 e2 65 b8 e5       	vfmadd231ps %ymm5,%ymm3,%ymm4
   10ae3:	c5 a4 59 eb          	vmulps %ymm3,%ymm11,%ymm5
   10ae7:	c4 e2 4d ba e8       	vfmsub231ps %ymm0,%ymm6,%ymm5
   10aec:	c5 cc 59 f7          	vmulps %ymm7,%ymm6,%ymm6
   10af0:	c4 e2 2d ba f3       	vfmsub231ps %ymm3,%ymm10,%ymm6
   10af5:	c5 ac 59 c0          	vmulps %ymm0,%ymm10,%ymm0
   10af9:	c4 e2 25 ba c7       	vfmsub231ps %ymm7,%ymm11,%ymm0
   10afe:	c5 c0 57 ff          	vxorps %xmm7,%xmm7,%xmm7
   10b02:	c5 8c 59 de          	vmulps %ymm6,%ymm14,%ymm3
   10b06:	c4 e2 1d b8 dd       	vfmadd231ps %ymm5,%ymm12,%ymm3
   10b0b:	c5 ec 59 f6          	vmulps %ymm6,%ymm2,%ymm6
   10b0f:	c5 fc 10 94 24 e0 00 	vmovups 0xe0(%rsp),%ymm2
   10b16:	00 00
   10b18:	c4 e2 35 b8 f5       	vfmadd231ps %ymm5,%ymm9,%ymm6
   10b1d:	c4 e2 7d b8 9c 24 a0 	vfmadd231ps 0xa0(%rsp),%ymm0,%ymm3
   10b24:	00 00 00
   10b27:	c4 62 7d 18 05 b0 45 	vbroadcastss 0x45b0(%rip),%ymm8        # 150e0 <_IO_stdin_used+0xe0>
   10b2e:	00 00
   10b30:	c4 c1 3c 5f ef       	vmaxps %ymm15,%ymm8,%ymm5
   10b35:	c4 62 7d 18 0d fa 44 	vbroadcastss 0x44fa(%rip),%ymm9        # 15038 <_IO_stdin_used+0x38>
   10b3c:	00 00
   10b3e:	c5 b4 5e ed          	vdivps %ymm5,%ymm9,%ymm5
   10b42:	c5 d4 59 e4          	vmulps %ymm4,%ymm5,%ymm4
   10b46:	c4 e2 15 b8 f0       	vfmadd231ps %ymm0,%ymm13,%ymm6
   10b4b:	c5 fc 10 8c 24 c0 00 	vmovups 0xc0(%rsp),%ymm1
   10b52:	00 00
   10b54:	c5 d4 59 c6          	vmulps %ymm6,%ymm5,%ymm0
   10b58:	c5 d4 59 db          	vmulps %ymm3,%ymm5,%ymm3
   10b5c:	c4 c1 3c c2 ef 02    	vcmpleps %ymm15,%ymm8,%ymm5
   10b62:	c5 c4 c2 f4 02       	vcmpleps %ymm4,%ymm7,%ymm6
   10b67:	c5 d4 54 ee          	vandps %ymm6,%ymm5,%ymm5
   10b6b:	c4 c1 5c c2 f1 02    	vcmpleps %ymm9,%ymm4,%ymm6
   10b71:	c5 c4 c2 f8 02       	vcmpleps %ymm0,%ymm7,%ymm7
   10b76:	c5 cc 54 f7          	vandps %ymm7,%ymm6,%ymm6
   10b7a:	c5 cc 54 ed          	vandps %ymm5,%ymm6,%ymm5
   10b7e:	c4 e2 7d 18 35 51 45 	vbroadcastss 0x4551(%rip),%ymm6        # 150d8 <_IO_stdin_used+0xd8>
   10b85:	00 00
   10b87:	c5 cc c2 f3 01       	vcmpltps %ymm3,%ymm6,%ymm6
   10b8c:	c5 cc 54 ed          	vandps %ymm5,%ymm6,%ymm5
   10b90:	c5 dc 58 f0          	vaddps %ymm0,%ymm4,%ymm6
   10b94:	c4 c1 4c c2 f1 02    	vcmpleps %ymm9,%ymm6,%ymm6
   10b9a:	c5 d4 54 ee          	vandps %ymm6,%ymm5,%ymm5
   10b9e:	c5 e4 c2 f1 01       	vcmpltps %ymm1,%ymm3,%ymm6
   10ba3:	c5 cc 54 ed          	vandps %ymm5,%ymm6,%ymm5
   10ba7:	c5 d5 72 f5 1f       	vpslld $0x1f,%ymm5,%ymm5
   10bac:	c4 e3 75 4a cb 50    	vblendvps %ymm5,%ymm3,%ymm1,%ymm1
   10bb2:	c4 e2 55 2e 64 24 a0 	vmaskmovps %ymm4,%ymm5,-0x60(%rsp)
   10bb9:	c4 e2 55 2e 44 24 80 	vmaskmovps %ymm0,%ymm5,-0x80(%rsp)
   10bc0:	c5 f9 6e c1          	vmovd  %ecx,%xmm0
   10bc4:	c4 e2 7d 58 c0       	vpbroadcastd %xmm0,%ymm0
   10bc9:	c4 e3 6d 4a d0 50    	vblendvps %ymm5,%ymm0,%ymm2,%ymm2
   10bcf:	48 ff c1             	inc    %rcx
   10bd2:	48 83 c7 48          	add    $0x48,%rdi
   10bd6:	48 39 c8             	cmp    %rcx,%rax
   10bd9:	0f 85 41 fe ff ff    	jne    10a20 <intersect_batch(SceneData const&, Ray const*, Hit*)+0x200>
   10bdf:	c4 e3 7d 19 cc 01    	vextractf128 $0x1,%ymm1,%xmm4
   10be5:	c5 fa 16 dc          	vmovshdup %xmm4,%xmm3
   10be9:	c5 d9 c6 c4 01       	vshufpd $0x1,%xmm4,%xmm4,%xmm0
   10bee:	eb 1d                	jmp    10c0d <intersect_batch(SceneData const&, Ray const*, Hit*)+0x3ed>
   10bf0:	c5 fa 10 05 e4 44 00 	vmovss 0x44e4(%rip),%xmm0        # 150dc <_IO_stdin_used+0xdc>
   10bf7:	00
   10bf8:	c5 ed 76 d2          	vpcmpeqd %ymm2,%ymm2,%ymm2
   10bfc:	c4 e2 7d 18 0d d7 44 	vbroadcastss 0x44d7(%rip),%ymm1        # 150dc <_IO_stdin_used+0xdc>
   10c03:	00 00
   10c05:	c5 f9 28 d8          	vmovapd %xmm0,%xmm3
   10c09:	c5 f9 28 e0          	vmovapd %xmm0,%xmm4
   10c0d:	c5 fa 10 6c 24 a0    	vmovss -0x60(%rsp),%xmm5
   10c13:	c5 fa 10 74 24 80    	vmovss -0x80(%rsp),%xmm6
   10c19:	c5 fa 11 0a          	vmovss %xmm1,(%rdx)
   10c1d:	c5 fa 11 6a 04       	vmovss %xmm5,0x4(%rdx)
   10c22:	c5 fa 11 72 08       	vmovss %xmm6,0x8(%rdx)
   10c27:	c5 f9 7e 52 0c       	vmovd  %xmm2,0xc(%rdx)
   10c2c:	c5 fa 10 6c 24 a4    	vmovss -0x5c(%rsp),%xmm5
   10c32:	c5 fa 10 74 24 84    	vmovss -0x7c(%rsp),%xmm6
   10c38:	c4 e3 79 17 4a 10 01 	vextractps $0x1,%xmm1,0x10(%rdx)
   10c3f:	c5 fa 11 6a 14       	vmovss %xmm5,0x14(%rdx)
   10c44:	c5 fa 11 72 18       	vmovss %xmm6,0x18(%rdx)
   10c49:	c4 e3 79 16 52 1c 01 	vpextrd $0x1,%xmm2,0x1c(%rdx)
   10c50:	c5 fa 10 6c 24 a8    	vmovss -0x58(%rsp),%xmm5
   10c56:	c5 fa 10 74 24 88    	vmovss -0x78(%rsp),%xmm6
   10c5c:	c4 e3 79 17 4a 20 02 	vextractps $0x2,%xmm1,0x20(%rdx)
   10c63:	c5 fa 11 6a 24       	vmovss %xmm5,0x24(%rdx)
   10c68:	c5 fa 11 72 28       	vmovss %xmm6,0x28(%rdx)
   10c6d:	c4 e3 79 16 52 2c 02 	vpextrd $0x2,%xmm2,0x2c(%rdx)
   10c74:	c5 fa 10 6c 24 ac    	vmovss -0x54(%rsp),%xmm5
   10c7a:	c5 fa 10 74 24 8c    	vmovss -0x74(%rsp),%xmm6
   10c80:	c4 e3 79 17 4a 30 03 	vextractps $0x3,%xmm1,0x30(%rdx)
   10c87:	c5 fa 11 6a 34       	vmovss %xmm5,0x34(%rdx)
   10c8c:	c5 fa 11 72 38       	vmovss %xmm6,0x38(%rdx)
   10c91:	c4 e3 79 16 52 3c 03 	vpextrd $0x3,%xmm2,0x3c(%rdx)
   10c98:	c5 fa 10 6c 24 b0    	vmovss -0x50(%rsp),%xmm5
   10c9e:	c5 fa 10 74 24 90    	vmovss -0x70(%rsp),%xmm6
   10ca4:	c4 e3 7d 39 d2 01    	vextracti128 $0x1,%ymm2,%xmm2
   10caa:	c5 fa 11 62 40       	vmovss %xmm4,0x40(%rdx)
   10caf:	c5 fa 11 6a 44       	vmovss %xmm5,0x44(%rdx)
   10cb4:	c5 fa 11 72 48       	vmovss %xmm6,0x48(%rdx)
   10cb9:	c5 f9 7e 52 4c       	vmovd  %xmm2,0x4c(%rdx)
   10cbe:	c5 fa 10 64 24 b4    	vmovss -0x4c(%rsp),%xmm4
   10cc4:	c5 fa 10 6c 24 94    	vmovss -0x6c(%rsp),%xmm5
   10cca:	c5 fa 11 5a 50       	vmovss %xmm3,0x50(%rdx)
   10ccf:	c5 fa 11 62 54       	vmovss %xmm4,0x54(%rdx)
   10cd4:	c5 fa 11 6a 58       	vmovss %xmm5,0x58(%rdx)
   10cd9:	c4 e3 79 16 52 5c 01 	vpextrd $0x1,%xmm2,0x5c(%rdx)
   10ce0:	c5 fa 10 5c 24 b8    	vmovss -0x48(%rsp),%xmm3
   10ce6:	c5 fa 10 64 24 98    	vmovss -0x68(%rsp),%xmm4
   10cec:	c5 fa 11 42 60       	vmovss %xmm0,0x60(%rdx)
   10cf1:	c5 fa 11 5a 64       	vmovss %xmm3,0x64(%rdx)
   10cf6:	c5 fa 11 62 68       	vmovss %xmm4,0x68(%rdx)
   10cfb:	c4 e3 79 16 52 6c 02 	vpextrd $0x2,%xmm2,0x6c(%rdx)
   10d02:	c4 e3 7d 19 c8 01    	vextractf128 $0x1,%ymm1,%xmm0
   10d08:	c5 fa 10 4c 24 bc    	vmovss -0x44(%rsp),%xmm1
   10d0e:	c5 fa 10 5c 24 9c    	vmovss -0x64(%rsp),%xmm3
   10d14:	c4 e3 79 17 42 70 03 	vextractps $0x3,%xmm0,0x70(%rdx)
   10d1b:	c5 fa 11 4a 74       	vmovss %xmm1,0x74(%rdx)
   10d20:	c5 fa 11 5a 78       	vmovss %xmm3,0x78(%rdx)
   10d25:	c4 e3 79 16 52 7c 03 	vpextrd $0x3,%xmm2,0x7c(%rdx)
   10d2c:	48 81 c4 08 01 00 00 	add    $0x108,%rsp
   10d33:	c5 f8 77             	vzeroupper
   10d36:	c3                   	ret
   10d37:	66 0f 1f 84 00 00 00 	nopw   0x0(%rax,%rax,1)
   10d3e:	00 00
