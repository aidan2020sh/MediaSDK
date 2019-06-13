// Copyright (c) 2003-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "umc_defs.h"
#if defined (UMC_ENABLE_AAC_AUDIO_DECODER)

#include "aaccmn_adif.h"
#include "bstream.h"
#include "mp4cmn_pce.h"

Ipp32s
dec_adif_header(sAdif_header* p_adif_header,sBitsreamBuffer* p_bs)
{

  Ipp32s i;

  p_adif_header->adif_id = Getbits(p_bs,16);
  p_adif_header->adif_id <<= 16;
  p_adif_header->adif_id += Getbits(p_bs,16);

  if (p_adif_header->adif_id != ADIF_SIGNATURE)
  {
    return 1;
  }
  p_adif_header->copyright_id_present = Getbits(p_bs,1);
  if (p_adif_header->copyright_id_present)
  {

    for (i = 0; i < LEN_COPYRIGHT_ID ; i++)
    {
      p_adif_header->copyright_id[i] = (Ipp8s)Getbits(p_bs,8);
    }
      p_adif_header->copyright_id[i] = 0;

  }
  p_adif_header->original_copy = Getbits(p_bs,1);
  p_adif_header->home = Getbits(p_bs,1);
  p_adif_header->bitstream_type = Getbits(p_bs,1);
  p_adif_header->bitrate = Getbits(p_bs,23);
  p_adif_header->num_program_config_elements = Getbits(p_bs,4);

  if (p_adif_header->bitstream_type == 0)
  {
    p_adif_header->adif_buffer_fullness = Getbits(p_bs,20);
  }

  for (i = 0; i < p_adif_header->num_program_config_elements + 1; i++)
  {
    dec_program_config_element(&p_adif_header->pce[i],p_bs);
  }

  return 0;
}

/***********************************************************************

    Unpack function(s) (support(s) alternative bitstream representation)

***********************************************************************/

Ipp32s
unpack_adif_header(sAdif_header* p_adif_header,Ipp8u **pp_bitstream, Ipp32s *p_offset)
{
  Ipp32s i;

  p_adif_header->adif_id = get_bits(pp_bitstream,p_offset,16);
  p_adif_header->adif_id <<= 16;
  p_adif_header->adif_id += get_bits(pp_bitstream,p_offset,16);

  if (p_adif_header->adif_id != ADIF_SIGNATURE)
  {
    return 1;
  }
  p_adif_header->copyright_id_present = get_bits(pp_bitstream,p_offset,1);
  if (p_adif_header->copyright_id_present)
  {
    for (i = 0; i < LEN_COPYRIGHT_ID ; i++)
    {
      p_adif_header->copyright_id[i] = (Ipp8s)get_bits(pp_bitstream,p_offset,8);
    }
      p_adif_header->copyright_id[i] = 0;

  }
  p_adif_header->original_copy = get_bits(pp_bitstream,p_offset,1);
  p_adif_header->home = get_bits(pp_bitstream,p_offset,1);
  p_adif_header->bitstream_type = get_bits(pp_bitstream,p_offset,1);
  p_adif_header->bitrate = get_bits(pp_bitstream,p_offset,23);
  p_adif_header->num_program_config_elements = get_bits(pp_bitstream,p_offset,4);

  if (p_adif_header->bitstream_type == 0)
  {
    p_adif_header->adif_buffer_fullness = get_bits(pp_bitstream,p_offset,20);
  }

  for (i = 0; i < p_adif_header->num_program_config_elements + 1; i++)
  {
    unpack_program_config_element(&p_adif_header->pce[i],pp_bitstream,p_offset);
  }

  return 0;
}

#else

#ifdef _MSVC_LANG
#pragma warning( disable: 4206 )
#endif

#endif //UMC_ENABLE_XXX
