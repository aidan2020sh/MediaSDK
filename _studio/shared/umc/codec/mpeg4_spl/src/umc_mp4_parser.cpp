//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_mp4_spl.h"
#include "umc_mp4_parser.h"

#include <ipps.h>

namespace UMC
{

Status MP4Splitter::Read_traf(DataReader *dr, T_traf *traf, T_atom_mp4 *atom)
{
    Status ret;
    T_atom_mp4    current_atom;

    traf->entry_count = 0;

    do
    {
        ret = Read_Atom(dr, &current_atom);
        UMC_CHECK_STATUS(ret)

        if (Compare_Atom(&current_atom, "tfhd"))
        {
            ret = Read_tfhd(dr, &traf->tfhd);
            UMC_CHECK_STATUS(ret)
            ret = Atom_Skip(dr, &current_atom);
            UMC_CHECK_STATUS(ret)
        }

        if (Compare_Atom(&current_atom, "trun"))
        {
            if (!traf->trun[traf->entry_count])
            {
                traf->trun[traf->entry_count] = (T_trun*) (ippsMalloc_8u(sizeof(T_trun)));
                memset(traf->trun[traf->entry_count], 0, sizeof(T_trun));
            }
            ret = Read_trun(dr, traf->trun[traf->entry_count]);
            UMC_CHECK_STATUS(ret)
            traf->entry_count++;
            ret = Atom_Skip(dr, &current_atom);
        }
    } while((dr->GetPosition() < atom->end) && (dr->GetPosition() != 0) && (ret == UMC_OK));

    if (traf->entry_count > traf->max_truns)
        traf->max_truns = traf->entry_count;

    return ret;
}
/*****

Status Read_traf(DataReader *dr, T_traf *traf, T_atom_mp4 *atom)
{
    Status ret;
    T_atom_mp4    current_atom;

    traf->entry_count = 0;

    do
    {
        ret = Read_Atom(m_pDataReader, &current_atom);
        UMC_CHECK_STATUS(ret)

        if (Compare_Atom(&current_atom, "tfhd"))
        {
            ret = Read_tfhd(m_pDataReader, &traf->tfhd);
            UMC_CHECK_STATUS(ret)
            ret = Atom_Skip(m_pDataReader, &current_atom);
            UMC_CHECK_STATUS(ret)
        }

        if (Compare_Atom(&current_atom, "trun"))
        {
            if (!traf->trun[traf->entry_count])
            {
                traf->trun[traf->entry_count] = (T_trun*) (ippsMalloc_8u(sizeof(T_trun)));
                memset(traf->trun[traf->entry_count], 0, sizeof(T_trun));
            }
            ret = Read_trun(m_pDataReader, traf->trun[traf->entry_count]);
            UMC_CHECK_STATUS(ret)
            traf->entry_count++;
            ret = Atom_Skip(m_pDataReader, &current_atom);
        }
    } while((dr->GetPosition() < atom->end) && (dr->GetPosition() != 0) && (ret == UMC_OK));

    if (traf->entry_count > traf->max_truns)
        traf->max_truns = traf->entry_count;

    return ret;
}
*/

Status MP4Splitter::Read_tfhd(DataReader *dr, T_tfhd *tfhd)
{
    dr->Get8u(&tfhd->version);
    tfhd->flags = Get_24(dr);
    dr->Get32uSwap(&tfhd->track_ID);

    if (tfhd->flags & 0x000001)
    {
        dr->Get64uSwap((Ipp64u*)&tfhd->base_data_offset);
    }
    else
    {
        tfhd->base_data_offset = 0;
    }

    if (tfhd->flags & 0x000002)
    {
        dr->Get32uSwap((Ipp32u*)&tfhd->sample_description_index);
    }
    else
    {
        tfhd->sample_description_index = 0;
    }

    if (tfhd->flags & 0x000008)
    {
        dr->Get32uSwap((Ipp32u*)&tfhd->default_sample_duration);
    }
    else
    {
        tfhd->default_sample_duration = 0;
    }

    if (tfhd->flags & 0x000010)
    {
        dr->Get32uSwap((Ipp32u*)&tfhd->default_sample_size);
    }
    else
    {
        tfhd->default_sample_size = 0;
    }

    if (tfhd->flags & 0x000020)
    {
        dr->Get32uSwap((Ipp32u*)&tfhd->default_sample_flags);
    }
    else
    {
        tfhd->default_sample_flags = 0;
    }

    return UMC_OK;
}

Status MP4Splitter::Read_trun(DataReader *dr, T_trun *trun)
{
    dr->Get8u(&trun->version);
    trun->flags = Get_24(dr);
    dr->Get32uSwap(&trun->sample_count);
    if (trun->flags & 0x000001)
    {
        dr->Get32uSwap((Ipp32u*)&trun->data_offset);
    }
    else
    {
        trun->data_offset = 0;
    }
    if (trun->flags & 0x000004)
    {
        dr->Get32uSwap((Ipp32u*)&trun->first_sample_flags);
    }
    else
    {
        trun->first_sample_flags = 0;
    }

    if (!trun->table_trun)
    {
        trun->table_trun = (T_trun_table_data*) (ippsMalloc_8u(sizeof(T_trun_table_data) * /*trun->sample_count*/40000));  // 40000 ~= number of entries in standard moof box
    }
    memset(trun->table_trun, 0, sizeof(T_trun_table_data) * /*trun->sample_count*/40000);
    return Read_trun_table(dr, trun, trun->table_trun);
    //return 0;
}

Status MP4Splitter::Read_trun_table(DataReader *dr, T_trun* trun, T_trun_table_data* table)
{
    for (Ipp32u  i = 0; i < trun->sample_count; i++)
    {
        if (trun->flags & 0x000100)
        {
            dr->Get32uSwap((Ipp32u*)&table[i].sample_duration);
        }

        if (trun->flags & 0x000200)
        {
            dr->Get32uSwap((Ipp32u*)&table[i].sample_size);
        }

        if (trun->flags & 0x000400)
        {
            dr->Get32uSwap((Ipp32u*)&table[i].sample_flags);
        }

        if (trun->flags & 0x000800)
        {
            dr->Get32uSwap((Ipp32u*)&table[i].sample_composition_time_offset);
        }
    }
    return 0;
}

Status MP4Splitter::Clear_moof(T_moof &moof)
{
  Ipp32u i, j;

  for (i = 0; i < moof.total_tracks; i++) {
    for (j = 0; j < moof.traf[i]->entry_count; j++) {
      UMC_FREE(moof.traf[i]->trun[j]->table_trun)
      UMC_FREE(moof.traf[i]->trun[j])
    }
    UMC_FREE(moof.traf[i])
  }

  return UMC_OK;
}

Status MP4Splitter::Read_moof(DataReader *dr, T_moof *moof, T_atom_mp4 *atom)
{
  Status     umcRes;
  T_atom_mp4 current_atom;

  moof->end = atom->end;
  moof->total_tracks = 0;

  do
  {
    umcRes = Read_Atom(dr, &current_atom);
    UMC_CHECK_STATUS(umcRes)

    if (Compare_Atom(&current_atom, "mfhd")) {
      umcRes = Read_mfhd(dr, &moof->mfhd);
      UMC_CHECK_STATUS(umcRes)
      umcRes = Atom_Skip(dr, &current_atom);
    } else if (Compare_Atom(&current_atom, "traf")) {
      if (!moof->traf[moof->total_tracks]) {
        moof->traf[moof->total_tracks] = (T_traf*)(ippsMalloc_8u(sizeof(T_traf)));
        memset(moof->traf[moof->total_tracks], 0, sizeof(T_traf));
      }
      umcRes = Read_traf(dr, moof->traf[moof->total_tracks], &current_atom);
      UMC_CHECK_STATUS(umcRes)

      umcRes = Atom_Skip(dr, &current_atom);
      UMC_CHECK_STATUS(umcRes)
      moof->total_tracks++;
    } else {
      return UMC_ERR_INVALID_STREAM;
    }

  } while((dr->GetPosition() < atom->end) && (dr->GetPosition() != 0) && (umcRes == UMC_OK));

    return umcRes;
}
/*****

Status Read_moof(DataReader *dr, T_moof *moof, T_atom_mp4 *atom)
{
    Status ret;
    T_atom_mp4    current_atom;

    moof->end = atom->end;
    moof->total_tracks = 0;

    do
    {
        ret = Read_Atom(m_pDataReader, &current_atom);
        UMC_CHECK_STATUS(ret)

        if (Compare_Atom(&current_atom, "mfhd"))
        {
            ret = Read_mfhd(m_pDataReader, &moof->mfhd);
            UMC_CHECK_STATUS(ret)
            ret = Atom_Skip(m_pDataReader, &current_atom);
        }
        else if (Compare_Atom(&current_atom, "traf"))
        {
            if (!moof->traf[moof->total_tracks])
            {
                moof->traf[moof->total_tracks] = (T_traf*) (ippsMalloc_8u(sizeof(T_traf)));
                memset(moof->traf[moof->total_tracks], 0, sizeof(T_traf));
            }
            ret = Read_traf(m_pDataReader, moof->traf[moof->total_tracks], &current_atom);
            UMC_CHECK_STATUS(ret)

            ret = Atom_Skip(m_pDataReader, &current_atom);
            UMC_CHECK_STATUS(ret)
            moof->total_tracks++;
        }
        else
        {
            return UMC_ERR_INVALID_STREAM;
        }

    }while((dr->GetPosition() < atom->end) && (dr->GetPosition() != 0) && (ret == UMC_OK));

    return ret;
}
*/

Ipp32u MP4Splitter::Get_24(DataReader *dr)
{
    Ipp32u     result = 0;
    Ipp32u     a, b, c;
    Ipp32u     len;
    char       data[3];

    len = 3;
    dr->GetData(data, &len);
    a = (Ipp8u )data[0];
    b = (Ipp8u )data[1];
    c = (Ipp8u )data[2];
    result = (a<<16) + (b<<8) + c;
    return result;
}

Ipp32s MP4Splitter::Read_mp4_descr_length(DataReader *dr)
{
    Ipp8u     b;
    Ipp8u            numBytes = 0;
    Ipp32s           length = 0;

    do
    {
        dr->Get8u(&b);
        numBytes++;
        length = (length << 7) | (b & 0x7F);
    } while ((b & 0x80) && numBytes < 4);

    return length;
}

/*****
Ipp32s Get_mp4_descr_length(DataReader *dr)
{
    Ipp8u     b;
    Ipp8u            numBytes = 0;
    Ipp32s           length = 0;

    do
    {
        dr->Get8u(&b);
        numBytes++;
        length = (length << 7) | (b & 0x7F);
    } while ((b & 0x80) && numBytes < 4);

    return length;
}
*/

Status MP4Splitter::Atom_Skip(DataReader *dr, T_atom_mp4 *atom)
{
#ifndef UMC_MF
  dr->SetPosition((Ipp64u)0);
  dr->StartReadingAfterReposition();
  return dr->MovePosition(atom->end);
#else
  return dr->SetPosition(atom->end);
#endif
}

bool MP4Splitter::Compare_Atom(T_atom_mp4 *atom, const char *type)
{
    if ( atom->type[0] == type[0] &&
        atom->type[1] == type[1] &&
        atom->type[2] == type[2] &&
        atom->type[3] == type[3] )
    {
        return true;
    }
    else
    {
        return false;
    }
}

Status MP4Splitter::Read_Atom(DataReader *dr, T_atom_mp4 *atom)
{
    Status ret;
    Ipp32u  len;
    Ipp32u  size = 0;
    atom->start = dr->GetPosition();
    dr->Get32uSwap(&size);
    atom->is_large_size = (size == 1);
    len = 4;
    ret = dr->GetData(&atom->type, (Ipp32u*) &len);
    if (ret != UMC_OK)
    {
        return UMC_ERR_FAILED;
    }

    if (size == 1)
    {
        ret = dr->Get64uSwap(&atom->size);
        if (ret != UMC_OK)
        {
            return UMC_ERR_FAILED;
        }
    }
    else
    {
        atom->size = size;
    }

    atom->end = atom->start + atom->size;

    if (atom->type[0] == 'u' &&
        atom->type[1] == 'u' &&
        atom->type[2] == 'i' &&
        atom->type[3] == 'd')
    {
        Ipp8u userType[16];
        len = 16;
        ret = dr->GetData(userType, (Ipp32u*)&len);
    }

    return ret;
}

T_trak_data* MP4Splitter::Add_trak(T_moov *moov)
{
    if ( moov->total_tracks < MAXTRACKS )
    {
        moov->trak[moov->total_tracks] = (T_trak_data*) (ippsMalloc_8u(sizeof(T_trak_data)));
        if (moov->trak[moov->total_tracks]) {

            memset(moov->trak[moov->total_tracks],  0, sizeof(T_trak_data));
            moov->total_tracks++;
        } else {
            return 0;
        }
    }
    else
    {
        return 0;
    }
    return moov->trak[moov->total_tracks - 1];
}

// MOOV Box
// The metadata for a presentation is stored in the single Movie Box which occurs at the top-level of a file.
//
Status MP4Splitter::Read_moov(DataReader *dr, T_moov *moov, T_atom_mp4 *atom)
{
    Status ret;
    T_atom_mp4    current_atom;

    moov->total_tracks = 0;
    do
    {
        ret = Read_Atom(dr, &current_atom);
        UMC_CHECK_STATUS(ret)

        if (Compare_Atom(&current_atom, "mvhd"))
        {
            ret = Read_mvhd(dr, &(moov->mvhd));
        }
        else if (Compare_Atom(&current_atom, "iods"))
        {
            ret = Read_iods(dr, &(moov->iods));
            UMC_CHECK_STATUS(ret)
            ret = Atom_Skip(dr, &current_atom);
        }
        else if (Compare_Atom(&current_atom, "mvex"))
        {
            ret = Read_mvex(dr, &moov->mvex, &current_atom);
            UMC_CHECK_STATUS(ret)
            ret = Atom_Skip(dr, &current_atom);
        }
        /*else if ( Compare_Atom(&current_atom, "clip") )
        {
            Atom_Skip(dr, &current_atom);
        }*/
        else if (Compare_Atom(&current_atom, "trak"))
        {
            T_trak_data *trak = Add_trak(moov);
            if (trak == 0)
            {
                return UMC_ERR_FAILED;
            }
            ret = Read_trak(dr, trak, &current_atom);
        }
        else
        {
            ret = Atom_Skip(dr, &current_atom);
        }
    } while ((dr->GetPosition() < atom->end) && (dr->GetPosition() != 0) && (ret == UMC_OK));

    return ret;
}

// MVHD Box
// This box defines overall information which is media-independent, and relevant to the entire presentation
// considered as a whole.
//
Status MP4Splitter::Read_mvhd(DataReader *dr, T_mvhd_data *mvhd)
{
    Status ret = UMC_OK;
    Ipp32u  temp;

    dr->Get8u(&mvhd->version);
    mvhd->flags = Get_24(dr);
    if ( mvhd->version )
    {
        dr->Get64uSwap(&mvhd->creation_time);
        dr->Get64uSwap(&mvhd->modification_time);
        dr->Get32uSwap(&mvhd->time_scale);
        dr->Get64uSwap(&mvhd->duration);
    }
    else
    {
        dr->Get32uSwap(&temp);
        mvhd->creation_time = temp;
        dr->Get32uSwap(&temp);
        mvhd->modification_time = temp;
        dr->Get32uSwap(&mvhd->time_scale);
        dr->Get32uSwap(&temp);
        mvhd->duration = temp;
    }
    dr->MovePosition((9 + 6) * 4 + 10 + 2 + 4); // RESERVED
    dr->Get32uSwap(&mvhd->next_track_id);

    return ret;
}

Status MP4Splitter::Read_iods(DataReader *dr, T_iods_data *iods)
{
    Status ret = UMC_OK;
    Ipp8u  tag;

    dr->Get8u(&iods->version);
    iods->flags = Get_24(dr );
    dr->Get8u(&tag);
//****    Get_mp4_descr_length(dr);    // skip length
    Read_mp4_descr_length(dr);    // skip length
    // skip ODID, ODProfile, sceneProfile
    dr->Get16uSwap(&iods->objectDescriptorID); // ODID (16 bit ?)
    dr->Get8u(&iods->OD_profileAndLevel); // ODProfile
    dr->Get8u(&iods->scene_profileAndLevel); // sceneProfile
    dr->Get8u(&iods->audioProfileId);
    dr->Get8u(&iods->videoProfileId);
    dr->Get8u(&iods->graphics_profileAndLevel);
    return ret;
}

// TRAK Box
// This is a container box for a single track of a presentation. A presentation consists of one or more tracks.
// Each track is independent of the other tracks in the presentation and carries its own temporal and spatial
// information. Each track will contain its associated Media Box.
//
Status MP4Splitter::Read_trak(DataReader *dr, T_trak_data *trak, T_atom_mp4 *trak_atom)
{
    Status ret;
    T_atom_mp4 leaf_atom;

    trak->tref.dpnd.idTrak = 0;

    do
    {
        ret = Read_Atom(dr, &leaf_atom);
        UMC_CHECK_STATUS(ret)

        if (Compare_Atom(&leaf_atom, "tkhd"))
        {
            ret = Read_tkhd(dr, &(trak->tkhd));
        }
        else if (Compare_Atom(&leaf_atom, "mdia"))
        {
            ret = Read_mdia(dr, &(trak->mdia), &leaf_atom);
        }
        else if ( Compare_Atom(&leaf_atom, "tref") )
        {
            ret = Read_tref(dr, &(trak->tref), &leaf_atom);
        }
        else
        {
            ret = Atom_Skip(dr, &leaf_atom);
        }
    } while ((dr->GetPosition() < trak_atom->end) && (dr->GetPosition() != 0) && (ret == UMC_OK));

    return ret;
}

// TREF Box
// This box provides a reference from the containing track to another track in the presentation. These references
// are typed.
//
Status MP4Splitter::Read_tref(DataReader *dr, T_tref_data *tref, T_atom_mp4 *atom)
{
    Status ret;
    T_atom_mp4 leaf_atom;

    do
    {
        ret = Read_Atom(dr, &leaf_atom);
        UMC_CHECK_STATUS(ret)

        if (Compare_Atom(&leaf_atom, "dpnd"))
        {
            ret = Read_dpnd(dr, &(tref->dpnd));
        }
        else
        {
            ret = Atom_Skip(dr, &leaf_atom);
        }
    } while ((dr->GetPosition() < atom->end) && (dr->GetPosition() != 0)  && (ret == UMC_OK));

    return ret;
}


Status MP4Splitter::Read_dpnd(DataReader *dr, T_dpnd_data *dpnd)
{
    dr->Get32uSwap(&dpnd->idTrak);
    return UMC_OK;
}

// TKHD Box
// This box specifies the characteristics of a single track.
//
Status MP4Splitter::Read_tkhd(DataReader *dr, T_tkhd_data *tkhd)
{
    Status ret = UMC_OK;
    Ipp32u  temp;

    dr->Get8u(&tkhd->version);
    tkhd->flags = Get_24(dr);
    if ( tkhd->version )
    {
        dr->Get64uSwap(&tkhd->creation_time);
        dr->Get64uSwap(&tkhd->modification_time);
        dr->Get32uSwap(&tkhd->track_id);
        dr->MovePosition(4); //RESERVED
        dr->Get64uSwap(&tkhd->duration);
    }
    else
    {
        dr->Get32uSwap(&temp);
        tkhd->creation_time = temp;
        dr->Get32uSwap(&temp);
        tkhd->modification_time = temp;
        dr->Get32uSwap(&tkhd->track_id);
        dr->MovePosition(4); //RESERVED
        dr->Get32uSwap(&temp);
        tkhd->duration = temp;
    }
    dr->MovePosition(16 + 9 * 4); //RESERVED
    dr->Get32uSwap(&temp);
    tkhd->track_width = (Ipp32f)(temp >> 16);
    dr->Get32uSwap(&temp);
    tkhd->track_height = (Ipp32f)(temp >> 16);

    return ret;
}

// MDIA
// The media declaration container contains all the objects that declare information about the media data within a
// track.
//
Status MP4Splitter::Read_mdia(DataReader *dr, T_mdia_data *mdia, T_atom_mp4 *trak_atom)
{
    Status ret;
    T_atom_mp4 leaf_atom;

    do
    {
        ret = Read_Atom(dr, &leaf_atom);
        UMC_CHECK_STATUS(ret)

        if (Compare_Atom(&leaf_atom, "mdhd"))
        {
            ret = Read_mdhd(dr, &(mdia->mdhd));
        }
        else if (Compare_Atom(&leaf_atom, "hdlr"))
        {
            ret = Read_hdlr(dr, &(mdia->hdlr));
            UMC_CHECK_STATUS(ret)
            ret = Atom_Skip(dr, &leaf_atom);
        }
        else if (Compare_Atom(&leaf_atom, "minf"))
        {
            ret = Read_minf(dr, &(mdia->minf), &leaf_atom);
        }
        else
        {
            ret = Atom_Skip(dr, &leaf_atom);
        }
    } while ((dr->GetPosition() < trak_atom->end) && (dr->GetPosition() != 0) && (ret == UMC_OK));

    return ret;
}

// HDLR Box
// This box within a Media Box declares the process by which the media-data in the track is presented, and thus,
// the nature of the media in a track. For example, a video track would be handled by a video handler.
//
Status MP4Splitter::Read_hdlr(DataReader *dr, T_hdlr_data *hdlr)
{
    Status ret = UMC_OK;
    Ipp32u  len;

    dr->Get8u(&hdlr->version);
    hdlr->flags = Get_24(dr);
    dr->MovePosition(4); // RESERVED
    len = 4;
    dr->GetData(hdlr->component_type, (Ipp32u*) &len);
    dr->MovePosition(12); // RESERVED

    return ret;
}

// MDHD Box
// The media header declares overall information that is media-independent, and relevant to characteristics of
// the media in a track.
//
Status MP4Splitter::Read_mdhd(DataReader *dr, T_mdhd_data *mdhd)
{
    Status ret = UMC_OK;
    Ipp32u  temp;

    dr->Get8u(&mdhd->version);
    mdhd->flags = Get_24(dr);
    if ( mdhd->version )
    {
        dr->Get64uSwap(&mdhd->creation_time);
        dr->Get64uSwap(&mdhd->modification_time);
        dr->Get32uSwap(&mdhd->time_scale);
        dr->Get64uSwap(&mdhd->duration);
    }
    else
    {
        dr->Get32uSwap(&temp);
        mdhd->creation_time = temp;
        dr->Get32uSwap(&temp);
        mdhd->modification_time = temp;
        dr->Get32uSwap(&mdhd->time_scale);
        dr->Get32uSwap(&temp);
        mdhd->duration = temp;
    }
    dr->Get16uSwap(&mdhd->language);
    dr->Get16uSwap(&mdhd->quality);    // RESERVED

    return ret;
}

// MINF Box
// This box contains all the objects that declare characteristic information of the media in the track.
//
Status MP4Splitter::Read_minf(DataReader *dr, T_minf_data *minf, T_atom_mp4 *parent_atom)
{
    Status ret;
    T_atom_mp4 leaf_atom;

    do
    {
        ret = Read_Atom(dr, &leaf_atom);
        UMC_CHECK_STATUS(ret)

        if (Compare_Atom(&leaf_atom, "vmhd"))
        {
            minf->is_video = 1;
            ret = Read_vmhd(dr, &(minf->vmhd));
        }
        else if (Compare_Atom(&leaf_atom, "smhd"))
        {
            minf->is_audio = 1;
            ret = Read_smhd(dr, &(minf->smhd));
        }
        else if (Compare_Atom(&leaf_atom, "hmhd"))
        {
            minf->is_hint = 1;
            ret = Read_hmhd(dr, &(minf->hmhd));
        }
        else if (Compare_Atom(&leaf_atom, "dinf"))
        {
            ret = Read_dinf(dr, &(minf->dinf), &leaf_atom);
        }
        else if (Compare_Atom(&leaf_atom, "stbl"))
        {
            ret = Read_stbl(dr, minf, &(minf->stbl), &leaf_atom);
        }
        else
        {
            ret = Atom_Skip(dr, &leaf_atom);
        }
    } while ( (dr->GetPosition() < parent_atom->end) && (dr->GetPosition() != 0) && (ret == UMC_OK));

    return ret;
}

// HMHD Box
// The hint media header contains general information, independent of the protocol, for hint tracks.
//
Status MP4Splitter::Read_hmhd(DataReader *dr, T_hmhd_data *hmhd)
{
    Status ret = UMC_OK;

    dr->Get8u(&hmhd->version);
    hmhd->flags = Get_24(dr);
    dr->Get16uSwap(&hmhd->maxPDUsize);
    dr->Get16uSwap(&hmhd->avgPDUsize);
    dr->Get32uSwap(&hmhd->maxbitrate);
    dr->Get32uSwap(&hmhd->avgbitrate);
    dr->Get32uSwap(&hmhd->slidingavgbitrate);

    return ret;
}

// VMHD Box
// The video media header contains general presentation information, independent of the coding, for video
// media.
//
Status MP4Splitter::Read_vmhd(DataReader *dr, T_vmhd_data *vmhd)
{
    Status ret = UMC_OK;

    dr->Get8u(&vmhd->version);
    vmhd->flags = Get_24(dr);
    dr->MovePosition(8);    // RESERVED

    return ret;
}

// SMHD Box
// The sound media header contains general presentation information, independent of the coding, for audio
// media. This header is used for all tracks containing audio.
//
Status MP4Splitter::Read_smhd(DataReader *dr, T_smhd_data *smhd)
{
    Status ret = UMC_OK;

    dr->Get8u(&smhd->version);
    smhd->flags = Get_24(dr);
    dr->MovePosition(4); // RESERVED

    return ret;
}

// STBL Box
// The sample table contains all the time and data indexing of the media samples in a track. Using the tables
// here, it is possible to locate samples in time, determine their type (e.g. I-frame or not), and determine their
// size, container, and offset into that container.
//
Status MP4Splitter::Read_stbl(DataReader *dr, T_minf_data *minf, T_stbl_data *stbl, T_atom_mp4 *parent_atom)
{
    Status ret;
    T_atom_mp4 leaf_atom;

    do
    {
        ret = Read_Atom(dr, &leaf_atom);
        UMC_CHECK_STATUS(ret)

        if (Compare_Atom(&leaf_atom, "stsd"))
        {
            ret = Read_stsd(dr, minf, &(stbl->stsd));
            if (ret == UMC_OK) {
                ret = Atom_Skip(dr, &leaf_atom);
            }
        }
        else if ( Compare_Atom(&leaf_atom, "stts") )
        {
            ret = Read_stts(dr, &(stbl->stts));
        }
        else if ( Compare_Atom(&leaf_atom, "stss") )
        {
            ret = Read_stss(dr, &(stbl->stss));
        }
        else if ( Compare_Atom(&leaf_atom, "stsc") )
        {
            ret = Read_stsc(dr, &(stbl->stsc));
        }
        else if ( Compare_Atom(&leaf_atom, "stsz") )
        {
            ret = Read_stsz(dr, &(stbl->stsz));
        }
        else if ( Compare_Atom(&leaf_atom, "stco") )
        {
            ret = Read_stco(dr, &(stbl->stco));
        }
        else if ( Compare_Atom(&leaf_atom, "co64") )
        {
            ret = Read_co64(dr, &(stbl->co64));
        }
        else if ( Compare_Atom(&leaf_atom, "ctts") )
        {
            ret = Read_ctts(dr, &(stbl->ctts));
        }
        else
        {
            ret = Atom_Skip(dr, &leaf_atom);
        }
    } while ( (dr->GetPosition() < parent_atom->end) && (dr->GetPosition() != 0) && (ret == UMC_OK));

    return ret;
}

// CO64 Box
// The chunk offset table gives the index of each chunk into the containing file. Use of 64-bit offsets.
//
Status MP4Splitter::Read_co64(DataReader *dr, T_co64_data *co64)
{
    Status ret = UMC_OK;
    Ipp32u  i;

    dr->Get8u(&co64->version);
    co64->flags = Get_24(dr);
    dr->Get32uSwap(&co64->total_entries);
    co64->table = (T_co64_table_data*)ippsMalloc_8u(sizeof(T_co64_table_data) * co64->total_entries);

    // Check memory allocation
    if (NULL == co64->table) {
        return ret;
    }

    // Zeroed allocated table

    memset(co64->table,  0, sizeof(T_co64_table_data) * co64->total_entries);


    for ( i = 0; i < co64->total_entries; i++ )
    {
        dr->Get64uSwap(&co64->table[i].offset);
    }

    return ret;
}

// CTTS Box
// This box provides the offset between decoding time and composition time.
//
Status MP4Splitter::Read_ctts(DataReader *dr, T_ctts_data *ctts)
{
    Status ret = UMC_OK;
    Ipp32u  i;

    dr->Get8u(&ctts->version);
    ctts->flags = Get_24(dr);
    dr->Get32uSwap(&ctts->total_entries);
    ctts->table = (T_ctts_table_data*)ippsMalloc_8u(sizeof(T_ctts_table_data) * ctts->total_entries);

    // Check memory allocation
    if (NULL == ctts->table) {
        return ret;
    }

    // Zeroed allocated table

    memset(ctts->table,  0, sizeof(T_ctts_table_data) * ctts->total_entries);

    for ( i = 0; i < ctts->total_entries; i++ )
    {
        dr->Get32uSwap(&ctts->table[i].sample_count);
        dr->Get32uSwap((Ipp32u*)&ctts->table[i].sample_offset);
    }

    return ret;
}

// STCO Box
// The chunk offset table gives the index of each chunk into the containing file. Use of 32-bit offsets.
//
Status MP4Splitter::Read_stco(DataReader *dr, T_stco_data *stco)
{
    Status ret = UMC_OK;
    Ipp32u  i;

    dr->Get8u(&stco->version);
    stco->flags = Get_24(dr);
    dr->Get32uSwap(&stco->total_entries);
    stco->table = (T_stco_table_data*)ippsMalloc_8u(sizeof(T_stco_table_data) * stco->total_entries);

    // Check memory allocation
    if (NULL == stco->table) {
        return ret;
    }

    // Zeroed allocated table

    memset(stco->table,  0, sizeof(T_stco_table_data) * stco->total_entries);

    for ( i = 0; i < stco->total_entries; i++ )
    {
        dr->Get32uSwap(&stco->table[i].offset);
    }

    return ret;
}

// STSZ Box
// This box contains the sample count and a table giving the size in bytes of each sample. This allows the media
// data itself to be unframed. The total number of samples in the media is always indicated in the sample count.
//
Status MP4Splitter::Read_stsz(DataReader *dr, T_stsz_data *stsz)
{
    Status ret = UMC_OK;
    Ipp32u  i;

    dr->Get8u(&stsz->version);
    stsz->flags = Get_24(dr);
    dr->Get32uSwap(&stsz->sample_size);
    dr->Get32uSwap(&stsz->total_entries);
    stsz->max_sample_size = stsz->sample_size;  // to calculate buffer size
    if (!stsz->sample_size) {
      stsz->table = (T_stsz_table_data*)ippsMalloc_8u(sizeof(T_stsz_table_data) * stsz->total_entries);

      // Check memory allocation
      if (NULL == stsz->table) {
          return ret;
      }

      // Zeroed allocated table

      memset(stsz->table,  0, sizeof(T_stsz_table_data) * stsz->total_entries);

      for (i = 0; i < stsz->total_entries; i++) {
        dr->Get32uSwap(&stsz->table[i].size);
        if (stsz->table[i].size > stsz->max_sample_size) {
            stsz->max_sample_size = stsz->table[i].size;
        }
      }
    }

    return ret;
}

// STSC Box
// Samples within the media data are grouped into chunks. Chunks can be of different sizes, and the samples
// within a chunk can have different sizes. This table can be used to find the chunk that contains a sample, its
// position, and the associated sample description.
//
Status MP4Splitter::Read_stsc(DataReader *dr, T_stsc_data *stsc)
{
    Status ret = UMC_OK;
    Ipp32u  i;
    Ipp32u  sample = 0;

    dr->Get8u(&stsc->version);
    stsc->flags = Get_24(dr);
    dr->Get32uSwap(&stsc->total_entries);
/////    stsc->entries_allocated = stsc->total_entries;
    stsc->table = (T_stsc_table_data*)ippsMalloc_8u(sizeof(T_stsc_table_data) * stsc->total_entries);

    // Check memory allocation
    if (NULL == stsc->table) {
        return ret;
    }

    // Zeroed allocated table

    memset(stsc->table,  0, sizeof(T_stsc_table_data) * stsc->total_entries);

    for ( i = 0; i < stsc->total_entries; i++ )
    {
        dr->Get32uSwap(&stsc->table[i].chunk);
        dr->Get32uSwap(&stsc->table[i].samples);
        dr->Get32uSwap(&stsc->table[i].id);
    }
    for ( i = 0; i < stsc->total_entries; i++ )
    {
        stsc->table[i].first_sample = sample;
        if ( i < stsc->total_entries - 1 )
        {
            sample += (stsc->table[i+1].chunk - stsc->table[i].chunk) * stsc->table[i].samples;
        }
    }

    return ret;
}

// STTS Box
// This box contains a compact version of a table that allows indexing from decoding time to sample number.
//
Status MP4Splitter::Read_stts(DataReader *dr, T_stts_data *stts)
{
    Status ret = UMC_OK;
    Ipp32u  i;

    dr->Get8u(&stts->version);
    stts->flags = Get_24(dr);
    dr->Get32uSwap(&stts->total_entries);
    stts->table = (T_stts_table_data*)ippsMalloc_8u(sizeof(T_stts_table_data) * stts->total_entries);

    // Check memory allocation
    if (NULL == stts->table) {
        return ret;
    }

    // Zeroed allocated table

    memset(stts->table,  0, sizeof(T_stts_table_data) * stts->total_entries);

    for( i = 0; i < stts->total_entries; i++ )
    {
        dr->Get32uSwap(&stts->table[i].sample_count);
        dr->Get32uSwap(&stts->table[i].sample_duration);
    }

    return ret;
}

// STSS Box
// This box contains a sync (key, I-frame) sample map.
//
Ipp32s MP4Splitter::Read_stss(DataReader *dr, T_stss_data *stss)
{
    Status ret = UMC_OK;
    Ipp32u i;

    dr->Get8u(&stss->version);
    stss->flags = Get_24(dr);
    dr->Get32uSwap(&stss->total_entries);
    stss->table = (T_stss_table_data*)ippsMalloc_8u(sizeof(T_stss_table_data) * stss->total_entries);

    // Check memory allocation
    if (NULL == stss->table) {
        return ret;
    }

    // Zeroed allocated table

    memset(stss->table,  0, sizeof(T_stss_table_data) * stss->total_entries);

    for( i = 0; i < stss->total_entries; i++ )
    {
        dr->Get32uSwap(&stss->table[i].sample_number);
        UMC_CHECK_STATUS(ret)
    }

    return ret;
}
// STSD Box
// The sample description table gives detailed information about the coding type used, and any initialization
// information needed for that coding.
//
Status MP4Splitter::Read_stsd(DataReader *dr, T_minf_data *minf, T_stsd_data *stsd)
{
    Status ret = UMC_OK;
    Ipp32u  i;

    dr->Get8u(&stsd->version);
    stsd->flags = Get_24(dr);
    dr->Get32uSwap(&stsd->total_entries);
    stsd->table = (T_stsd_table_data*)ippsMalloc_8u(sizeof(T_stsd_table_data) * stsd->total_entries);

    // Check memory allocation
    if (NULL == stsd->table)
    {
        return ret;
    }

    // Zeroed allocated table
    memset(stsd->table,  0, sizeof(T_stsd_table_data) * stsd->total_entries);

    for ( i = 0; i < stsd->total_entries; i++ )
    {
        ret = Read_stsd_table(dr, minf, &(stsd->table[i]));
        UMC_CHECK_STATUS(ret)
    }

    return ret;
}

Status MP4Splitter::Read_samr_audio(DataReader *dr,
                                    T_stsd_table_data *table)
{
  Status ret;
  T_atom_mp4 leaf_atom;
  Ipp16u i;
  dr->MovePosition(16); // reserved
  dr->Get16uSwap(&i);
  dr->MovePosition(2); // reserved

  ret = Read_Atom(dr, &leaf_atom);
  UMC_CHECK_STATUS(ret)

  if (leaf_atom.type[0] == 'd' && leaf_atom.type[1] == 'a' &&
      leaf_atom.type[2] == 'm' && leaf_atom.type[3] =='r') {
    return Read_damr_audio(dr, table, &leaf_atom);
  }

  return ret;
}

Status MP4Splitter::Read_damr_audio(DataReader *dr,
                                    T_stsd_table_data *table,
                                    T_atom_mp4 *parent_atom)
{
  Ipp32u    temp;

  table->damr.decoderConfigLen = (Ipp32s)(parent_atom->size - 8);
  table->damr.decoderConfig = (Ipp8u*)(ippsMalloc_8u(table->damr.decoderConfigLen));

  // Check memory allocation
  if (NULL == table->damr.decoderConfig) {
    return UMC_OK;
  }

  // Zeroed allocated table
  memset(table->damr.decoderConfig,  0, table->damr.decoderConfigLen);

  temp = table->damr.decoderConfigLen;
  dr->GetData(table->damr.decoderConfig, (Ipp32u*) &temp);

  return UMC_OK;
}

Status MP4Splitter::Read_stsd_table(DataReader *dr, T_minf_data * /*minf*/, T_stsd_table_data *table)
{
    Status ret;
    T_atom_mp4 leaf_atom;

    ret = Read_Atom(dr, &leaf_atom);
    UMC_CHECK_STATUS(ret)

    table->is_protected = 0;
    table->format[0] = leaf_atom.type[0];
    table->format[1] = leaf_atom.type[1];
    table->format[2] = leaf_atom.type[2];
    table->format[3] = leaf_atom.type[3];
    dr->MovePosition(6); // RESERVED
    dr->Get16uSwap(&table->data_reference);
    if ( table->format[0]=='m'&& table->format[1]=='p'&& table->format[2]=='4'&& table->format[3]=='a' )
    {
        ret = Read_stsd_audio(dr, table, &leaf_atom);
    }
    if ( table->format[0]=='s'&& table->format[1]=='a'&& table->format[2]=='m'&& table->format[3]=='r' )
    {
        ret = Read_samr_audio(dr, table);
    }
    if ( table->format[0]=='m'&& table->format[1]=='p'&& table->format[2]=='4'&& table->format[3]=='v' )
    {
        ret = Read_stsd_video(dr, table, &leaf_atom);
    }
    if ( table->format[0]=='m'&& table->format[1]=='p'&& table->format[2]=='4'&& table->format[3]=='s' )
    {
        ret = Read_stsd_hint(dr, table, &leaf_atom);
    }

    if ( table->format[0]=='d' && table->format[1]=='r' && table->format[2]=='m' && table->format[3]=='s' )
    {
        ret = Read_stsd_drms(dr, table, &leaf_atom);
    }

    if ( table->format[0]=='s'&& table->format[1]=='2'&& table->format[2]=='6'&& table->format[3]=='3' )
    {
        ret = Read_h263_video(dr, table);
        table->esds.objectTypeID = 0xf2;
    }

    if ( table->format[0]=='a'&& table->format[1]=='v'&& table->format[2]=='c'&& table->format[3]=='1' )
    {
        ret = Read_h264_video(dr, table, &leaf_atom);
        table->esds.objectTypeID = 0xf1;
    }
    if ( table->format[0]=='a'&& table->format[1]=='v'&& table->format[2]=='s'&& table->format[3]=='2' )
    {
        ret = Read_stsd_video(dr, table, &leaf_atom);
        table->esds.objectTypeID = 0xf4;
    }

    if ( table->format[0]=='h'&& table->format[1]=='v'&& table->format[2]=='c'&& table->format[3]=='1' )
    {
        ret = Read_h264_video(dr, table, &leaf_atom); // this suites hvcC at the moment
        table->esds.objectTypeID = 0xf5;
    }

    return ret;
}

Status MP4Splitter::Read_stsd_drms(UMC::DataReader *dr, T_stsd_table_data *table, T_atom_mp4 *parent_atom)
{
    Status ret = UMC_OK;
    Ipp16u sample_rate;

    table->is_protected = 1;

    dr->MovePosition(8); // reserved

    dr->Get16uSwap(&table->channels);
    dr->Get16uSwap(&table->sample_size);

    dr->MovePosition(4); // reserved

    dr->Get16uSwap(&sample_rate);
    table->sample_rate = sample_rate;

    dr->MovePosition(2); // reserved

    while ( (dr->GetPosition() < parent_atom->end) && (dr->GetPosition() != 0) && (ret == UMC_OK))
    {
        T_atom_mp4 leaf_atom;
        ret = Read_Atom(dr, &leaf_atom);
        UMC_CHECK_STATUS(ret)

        if (Compare_Atom(&leaf_atom, "esds"))
        {
            ret = Read_esds(dr, &(table->esds));
            UMC_CHECK_STATUS(ret)
            ret = Atom_Skip(dr, &leaf_atom);
        }
        else
        {
            ret = Atom_Skip(dr, &leaf_atom);
        }
    }

    return ret;
} // int Read_stsd_drms(UMC::DataReader *dr, T_stsd_table_data *table, T_atom_mp4 *parent_atom)
// STSD_HINT
// For hint tracks, the sample description contains appropriate declarative data for the streaming protocol being
// used, and the format of the hint track. The definition of the sample description is specific to the protocol.
//
Status MP4Splitter::Read_stsd_hint(DataReader *dr, T_stsd_table_data *table, T_atom_mp4 *parent_atom)
{
    Status ret = UMC_OK;

    //dr->MovePosition(16); // RESERVED
    while ((dr->GetPosition() < parent_atom->end) && (dr->GetPosition() != 0) && (ret == UMC_OK))
    {
        T_atom_mp4 leaf_atom;
        ret = Read_Atom(dr, &leaf_atom);
        UMC_CHECK_STATUS(ret)

        if ( Compare_Atom(&leaf_atom, "esds") )
        {
            ret = Read_esds(dr, &(table->esds));
            UMC_CHECK_STATUS(ret)
            ret = Atom_Skip(dr, &leaf_atom);
        }
        else
        {
            ret = Atom_Skip(dr, &leaf_atom);
        }
    }

    return ret;
}

// STSD_VIDEO
// For video tracks, a VisualSampleEntry is used.
//
Status MP4Splitter::Read_stsd_video(DataReader *dr, T_stsd_table_data *table, T_atom_mp4 *parent_atom)
{
    Status ret = UMC_OK;

    dr->MovePosition(2 + 2 + 3*4);
    table->width = 0;
    table->height = 0;
    dr->Get16uSwap(&table->width);
    dr->Get16uSwap(&table->height);
    dr->Get32uSwap(&table->dpi_horizontal);
    dr->Get32uSwap(&table->dpi_vertical);
    dr->MovePosition(4 + 2 + 32 + 2 + 2);

    while ((dr->GetPosition() < parent_atom->end) && (dr->GetPosition() != 0) && (ret == UMC_OK))
    {
        T_atom_mp4 leaf_atom;
        ret = Read_Atom(dr, &leaf_atom);
        UMC_CHECK_STATUS(ret)

        if (Compare_Atom(&leaf_atom, "esds"))
        {
            ret = Read_esds(dr, &(table->esds));
            UMC_CHECK_STATUS(ret)
            ret = Atom_Skip(dr, &leaf_atom);
        }
        else
        {
            ret = Atom_Skip(dr, &leaf_atom);
        }
    }

    return ret;
    //return 0;
}

Status MP4Splitter::Read_h263_video(DataReader *dr, T_stsd_table_data *table)
{
    dr->MovePosition(16);

    table->width = 0;
    table->height = 0;

    dr->Get16uSwap(&table->width);
    dr->Get16uSwap(&table->height);

    dr->MovePosition(39);
    dr->MovePosition(8);

    return UMC_OK;
}

void MP4Splitter::SetH264FrameIntraSize(Ipp8u* decoderConfig)
{
    Ipp8u *p = decoderConfig;
    p += 4;
    m_nH264FrameIntraSize = *p;
    m_nH264FrameIntraSize <<= 6;
    m_nH264FrameIntraSize >>= 6; // remove reserved bits
    m_nH264FrameIntraSize++;
    return;
};

Ipp32s MP4Splitter::Read_h264_video(DataReader *dr, T_stsd_table_data *table, T_atom_mp4 *parent_atom)
{
    Status ret = UMC_OK;

    dr->MovePosition(16);

    table->width = 0;
    table->height = 0;

    dr->Get16uSwap(&table->width);
    dr->Get16uSwap(&table->height);

    dr->MovePosition(50);

    while ((dr->GetPosition() < parent_atom->end) && (dr->GetPosition() != 0) && (ret == UMC_OK))
    {
        T_atom_mp4 leaf_atom;
        bool is_hevc = false;

        ret = Read_Atom(dr, &leaf_atom);
        UMC_CHECK_STATUS(ret)

        if (Compare_Atom(&leaf_atom, "avcC") || (true == (is_hevc = Compare_Atom(&leaf_atom, "hvcC"))))
        {
            Ipp32u    temp;

            table->avcC.type = (is_hevc)? HEVC_VIDEO: H264_VIDEO;
            table->avcC.decoderConfigLen = (Ipp32s)(leaf_atom.size - 8);
            table->avcC.decoderConfig = (Ipp8u*)(ippsMalloc_8u(table->avcC.decoderConfigLen));

            // Check memory allocation
            if (NULL == table->avcC.decoderConfig && table->avcC.decoderConfigLen > 0)
            {
                return UMC_ERR_FAILED;
            }
            // Zeroed allocated table
            memset(table->avcC.decoderConfig,  0, table->avcC.decoderConfigLen);

            temp = table->avcC.decoderConfigLen;
            dr->GetData(table->avcC.decoderConfig, (Ipp32u*) &temp);

            if (temp > 4)
                SetH264FrameIntraSize(table->avcC.decoderConfig);
 //*****               SetH264FrameIntraSize(table->avcC.decoderConfig, m_nH264FrameIntraSize);

            ret = Atom_Skip(dr, &leaf_atom);
        }
        else if (Compare_Atom(&leaf_atom, "btrt"))
        {
            dr->Get32uSwap((Ipp32u*)&table->esds.bufferSizeDB);
            dr->Get32uSwap(&table->esds.maxBitrate);
            dr->Get32uSwap(&table->esds.avgBitrate);
            ret = Atom_Skip(dr, &leaf_atom);
        }
        else if ( Compare_Atom(&leaf_atom, "m4ds") )
        {
            ret = Atom_Skip(dr, &leaf_atom);
        }
    }

    return ret;
}

Status MP4Splitter::Read_stsd_audio(DataReader *dr, T_stsd_table_data *table, T_atom_mp4 *parent_atom)
{
    Status ret = UMC_OK;

    dr->MovePosition(8); // RESERVED
    dr->Get16uSwap(&table->channels);
    dr->Get16uSwap(&table->sample_size);
    dr->MovePosition(4); // RESERVED
    dr->Get32uSwap(&table->sample_rate);
    table->sample_rate = table->sample_rate >> 16;

    while ((dr->GetPosition() < parent_atom->end) && (dr->GetPosition() != 0) && (ret == UMC_OK))
    {
        T_atom_mp4 leaf_atom;
        ret = Read_Atom(dr, &leaf_atom);
        UMC_CHECK_STATUS(ret)

        if (Compare_Atom(&leaf_atom, "esds"))
        {
            ret = Read_esds(dr, &(table->esds));
            UMC_CHECK_STATUS(ret)
            ret = Atom_Skip(dr, &leaf_atom);
        }
        else
        {
            ret = Atom_Skip(dr, &leaf_atom);
        }
    }

    return ret;
}

Status MP4Splitter::Read_esds(DataReader *dr, T_esds_data *esds)
{
    Ipp8u  tag;
    Ipp8u  temp2 = 0;
    int len;
    Ipp32u  temp;

    dr->Get8u(&esds->version);
    esds->flags = Get_24(dr);

    esds->decoderConfigLen = 0;
    /* get and verify ES_DescrTag */
    dr->Get8u(&tag);
    if ( tag == 0x03 )
    {
        len = Read_mp4_descr_length(dr);
        dr->Get16uSwap(&esds->es_ID);
        dr->Get8u(&temp2);
        esds->streamDependenceFlag = temp2 & (1 << 7) ? 1 : 0;
        esds->urlflag = temp2 & (1 << 6) ? 1 : 0;
        esds->ocrflag = temp2 & (1 << 5) ? 1 : 0;
        esds->streamPriority = temp2 & 0x1f;
        len -= 3;
        while ( len > 0 )
        {
            dr->Get8u(&tag);
            len -= 1;
            switch ( tag )
            {
                case 0x04: // verify DecoderConfigDescrTag
                {
                    Read_mp4_descr_length(dr);
                    dr->Get8u(&esds->objectTypeID);
                    dr->Get8u(&esds->streamType);    // 3 - video, 4 - graphic, 5 - audio
                    esds->bufferSizeDB = Get_24(dr);
                    dr->Get32uSwap(&esds->maxBitrate);
                    dr->Get32uSwap(&esds->avgBitrate);
                    len -= 17;
                    if (0 == len) break;

                    dr->Get8u(&tag);

                    if ( tag == 0x05 )
                    {
                        esds->decoderConfigLen = Read_mp4_descr_length(dr);
                        if ( esds->decoderConfigLen )
                        {
                            esds->decoderConfig = (Ipp8u*)(ippsMalloc_8u(esds->decoderConfigLen));

                            // Check memory allocation
                            if (NULL == esds->decoderConfig) {
                                return UMC_OK;
                            }

                            // Zeroed allocated table

                            memset(esds->decoderConfig,  0, esds->decoderConfigLen);

                            esds->start = dr->GetPosition();
                            temp = esds->decoderConfigLen;
                            dr->GetData(esds->decoderConfig, (Ipp32u*) &temp);
                            len -= 4 + temp;
                        }
                        else
                        {
                            esds->decoderConfigLen = 0;
                            len -= 4;
                        }
                    }
                    break;
                }
                case 0x06: // verify DecSpecificInfoTag
                {
                    temp = Read_mp4_descr_length(dr);
                    dr->MovePosition(temp);    // skip SL tag
                    len -= 4 + temp;
                    break;
                }
            } // switch tag
        } // while
    } // if (tag)
    else
    {
        return UMC_OK;
    }

    return UMC_OK;
}

// DINF Box
// The data information box contains objects that declare the location of the media information in a track.
//
Status MP4Splitter::Read_dinf(DataReader *dr, T_dinf_data *dinf, T_atom_mp4 *dinf_atom)
{
    Status ret;
    T_atom_mp4 leaf_atom;

    do
    {
        ret = Read_Atom(dr, &leaf_atom);
        UMC_CHECK_STATUS(ret)

        if(Compare_Atom(&leaf_atom, "dref"))
        {
            ret = Read_dref(dr, &(dinf->dref));
            UMC_CHECK_STATUS(ret)
        }
        else
        {
            ret = Atom_Skip(dr, &leaf_atom);
            UMC_CHECK_STATUS(ret)
        }
    } while ((dr->GetPosition() < dinf_atom->end) && (dr->GetPosition() != 0) && (ret == UMC_OK));

    return ret;
}


// DREF Box
// The data reference object contains a table of data references (normally URLs) that declare the location(s) of
// the media data used within the presentation.
//
Status MP4Splitter::Read_dref(DataReader *dr, T_dref_data *dref)
{
    Status ret = UMC_OK;
    Ipp32u  i;

    dr->Get8u(&dref->version);
    dref->flags = Get_24(dr);
    dr->Get32uSwap(&dref->total_entries);
    dref->table = (T_dref_table_data*)ippsMalloc_8u(sizeof(T_dref_table_data) * dref->total_entries);

    // Check memory allocation
    if (NULL == dref->table) {
        return ret;
    }

    // Zeroed allocated table
    memset(dref->table,  0, sizeof(T_dref_table_data) * dref->total_entries);

    for ( i = 0; i < dref->total_entries; i++ )
    {
        ret = Read_dref_table(dr, &(dref->table[i]));
        UMC_CHECK_STATUS(ret)
    }

    return ret;
}

Status MP4Splitter::Read_dref_table(DataReader *dr, T_dref_table_data *table)
{
    Status ret = UMC_OK;
    Ipp32u  len;

    dr->Get32uSwap(&table->size);
    len = 4;
    dr->GetData(table->type, (Ipp32u*) &len);
    dr->Get8u(&table->version);
    table->flags = Get_24(dr);
    table->data_reference = (char*)ippsMalloc_8u(table->size);

    // Check memory allocation
    if (NULL == table->data_reference) {
        return ret;
    }

    // Zeroed allocated table
    memset(table->data_reference,  0, table->size);

    len = table->size - 12;
    if ( table->size > 12 )
    {
        dr->GetData(table->data_reference, (Ipp32u*) &len);
    }
    table->data_reference[table->size - 12] = 0;

    return ret;
}

Status MP4Splitter::Read_mvex(DataReader *dr, T_mvex_data *mvex, T_atom_mp4 *atom)
{
    Status ret;
    T_atom_mp4    current_atom;

    mvex->total_tracks = 0;

    do
    {
        ret = Read_Atom(dr, &current_atom);
        UMC_CHECK_STATUS(ret)

        if (Compare_Atom(&current_atom, "trex"))
        {
            mvex->trex[mvex->total_tracks] = (T_trex_data*) (ippsMalloc_8u(sizeof(T_trex_data)));
            ret = Read_trex(dr, (mvex->trex[mvex->total_tracks]));
            UMC_CHECK_STATUS(ret)
            ret = Atom_Skip(dr, &current_atom);
            mvex->total_tracks++;
        }

    } while((dr->GetPosition() < atom->end) && (dr->GetPosition() != 0) && (ret == UMC_OK));

    return ret;
}

Status MP4Splitter::Read_trex(DataReader *dr, T_trex_data *trex)
{
    dr->Get8u(&trex->version);
    trex->flags = Get_24(dr);
    dr->Get32uSwap(&trex->track_ID);
    dr->Get32uSwap(&trex->default_sample_description_index);
    dr->Get32uSwap(&trex->default_sample_duration);
    dr->Get32uSwap(&trex->default_sample_size);
    dr->Get32uSwap(&trex->default_sample_flags);

    return UMC_OK;
}

Status MP4Splitter::Read_mfhd(DataReader *dr, T_mfhd *mfhd)
{
  dr->Get8u(&mfhd->version);
  mfhd->flags = Get_24(dr);
  dr->Get32uSwap(&mfhd->sequence_number);

  return UMC_OK;
}

} // namespace UMC
