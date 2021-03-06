/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/** Cheap and cheesy replacement code for liblangtag on systems that do not
    allow / want LGPL code or dependencies on glib.

    XXX NOTE: This code does not check language tags for validity or if they
    are registered with IANA, does not canonicalize or strip default script
    tags if included nor does it do any other fancy stuff that liblangtag is
    capable of. It just makes depending code work without.
 */

#include <cstdlib>
#include <cstring>
#include <cstdio>

namespace {

typedef void* lt_pointer_t;

struct lt_error_t {
    void *something;
    lt_error_t() : something(nullptr) {}
};

void* g_malloc(size_t s)
{
    return malloc(s);
}

void g_free(void* p)
{
    if (p)
        free(p);
}

void lt_error_unref(lt_error_t *error)
{
    if (error)
    {
        g_free( error->something);
        g_free( error);
    }
}

struct my_ref
{
    sal_uInt32 mnRef;
    explicit my_ref() : mnRef(1) {}
    virtual ~my_ref() {}
    void incRef() { ++mnRef; }
    void decRef() { if (--mnRef == 0) delete this; }
};

struct my_t_impl : public my_ref
{
    char* mpStr;
    explicit my_t_impl() : my_ref(), mpStr(nullptr) {}
    virtual ~my_t_impl() override { g_free( mpStr); }
    explicit my_t_impl( const my_t_impl& r )
        :
            my_ref(),
            mpStr(r.mpStr ? strdup( r.mpStr) : nullptr)
    {
    }
    my_t_impl& operator=( const my_t_impl& r )
    {
        if (this == &r)
            return *this;
        g_free( mpStr);
        mpStr = (r.mpStr ? strdup( r.mpStr) : nullptr);
        return *this;
    }
    void assign( const char* str )
    {
        g_free( mpStr);
        mpStr = (str ? strdup( str) : nullptr);
    }
    void assign( const char* str, const char* stop )
    {
        g_free( mpStr);
        if (str && str < stop)
        {
            mpStr = static_cast<char*>(g_malloc( stop - str + 1));
            memcpy( mpStr, str, stop - str);
            mpStr[stop - str] = 0;
        }
        else
            mpStr = nullptr;
    }
    virtual void append( const char* str, const char* stop )
    {
        if (str && str < stop)
        {
            size_t nOld = mpStr ? strlen( mpStr) : 0;
            size_t nNew = nOld + (stop - str) + 1;
            char* p = static_cast<char*>(g_malloc( nNew));
            if (nOld)
                memcpy( p, mpStr, nOld);
            memcpy( p + nOld, str, stop - str);
            p[nNew-1] = 0;
            g_free( mpStr);
            mpStr = p;
        }
    }
    virtual void zero()
    {
        g_free( mpStr);
        mpStr = nullptr;
    }
};

struct lt_lang_t : public my_t_impl
{
    explicit lt_lang_t() : my_t_impl() {}
    virtual ~lt_lang_t() override {}
};

struct lt_script_t : public my_t_impl
{
    explicit lt_script_t() : my_t_impl() {}
    virtual ~lt_script_t() override {}
};

struct lt_region_t : public my_t_impl
{
    explicit lt_region_t() : my_t_impl() {}
    virtual ~lt_region_t() override {}
};

struct lt_variant_t : public my_t_impl
{
    explicit lt_variant_t() : my_t_impl() {}
    virtual ~lt_variant_t() override {}
};

struct lt_string_t : public my_t_impl
{
    explicit lt_string_t() : my_t_impl() {}
    virtual ~lt_string_t() override {}
};

struct lt_list_t : public my_t_impl
{
    lt_list_t* mpPrev;
    lt_list_t* mpNext;
    explicit lt_list_t() : my_t_impl(), mpPrev(nullptr), mpNext(nullptr) {}
    explicit lt_list_t( const lt_list_t& r )
        :
            my_t_impl( r), mpPrev(nullptr), mpNext(nullptr)
    {
    }
    virtual ~lt_list_t() override
    {
        if (mpPrev)
            mpPrev->mpNext = mpNext;
        if (mpNext)
            mpNext->mpPrev = mpPrev;
    }
};

lt_pointer_t lt_list_value( const lt_list_t* p )
{
    // This may look odd, but in this implementation the list element itself
    // holds the char* mpStr to be obtained with lt_variant_get_tag()
    return static_cast<lt_pointer_t>(const_cast<lt_list_t*>(p));
}

const lt_list_t* lt_list_next( const lt_list_t* p )
{
    return p ? p->mpNext : nullptr;
}

lt_list_t* my_copyList( const lt_list_t * pList )
{
    lt_list_t* pNewList = nullptr;
    lt_list_t* pLast = nullptr;
    while (pList)
    {
        lt_list_t* p = new lt_list_t( *pList);
        if (!pNewList)
            pNewList = p;
        if (pLast)
        {
            pLast->mpNext = p;
            p->mpPrev = pLast;
        }
        pLast = p;
        pList = pList->mpNext;
    }
    return pNewList;
}

void my_unrefList( lt_list_t* pList )
{
    while (pList)
    {
        lt_list_t* pNext = pList->mpNext;
        pList->decRef();
        pList = pNext;
    }
}

void my_appendToList( lt_list_t** ppList, lt_list_t* pEntry )
{
    if (ppList)
    {
        if (!*ppList)
            *ppList = pEntry;
        else
        {
            lt_list_t* pThat = *ppList;
            for (lt_list_t* pNext = pThat->mpNext; pNext; pNext = pThat->mpNext)
                pThat = pNext;
            pThat->mpNext = pEntry;
            pEntry->mpPrev = pThat;
        }
    }
}

// my_t_impl has a superfluous mpStr here, but simplifies things much in the
// parser.
struct my_t_list : public my_t_impl
{
    lt_list_t* mpList;
    explicit my_t_list() : my_t_impl(), mpList(nullptr) {}
    explicit my_t_list( const my_t_list& r ) : my_t_impl( r), mpList( my_copyList( r.mpList)) {}
    virtual ~my_t_list() override
    {
        my_unrefList( mpList);
    }
    my_t_list& operator=( const my_t_list& r )
    {
        if (this == &r)
            return *this;
        my_t_impl::operator=( r);
        lt_list_t* pList = my_copyList( r.mpList);
        my_unrefList( mpList);
        mpList = pList;
        return *this;
    }
    virtual void append( const char* str, const char* stop ) override
    {
        lt_list_t* p = new lt_list_t;
        p->assign( str, stop);
        my_appendToList( &mpList, p);
    }
    virtual void zero() override
    {
        my_t_impl::zero();
        my_unrefList( mpList);
        mpList = nullptr;
    }
};

struct lt_tag_t : public my_t_impl
{
    lt_lang_t   maLanguage;
    lt_script_t maScript;
    lt_region_t maRegion;
    my_t_list   maVariants;
    lt_string_t maPrivateUse;
    explicit lt_tag_t() : my_t_impl(), maLanguage(), maScript(), maRegion(), maVariants() {}
    virtual ~lt_tag_t() override {}
    explicit lt_tag_t( const lt_tag_t& r )
        :
            my_t_impl( r),
            maLanguage( r.maLanguage),
            maScript( r.maScript),
            maRegion( r.maRegion),
            maVariants( r.maVariants)
    {
    }
    lt_tag_t& operator=( const lt_tag_t& r )
    {
        if (this == &r)
            return *this;
        my_t_impl::operator=( r);
        maLanguage = r.maLanguage;
        maScript = r.maScript;
        maRegion = r.maRegion;
        maVariants = r.maVariants;
        return *this;
    }
    void assign( const char* str )
    {
        maLanguage.zero();
        maScript.zero();
        maRegion.zero();
        maVariants.zero();
        my_t_impl::assign( str);
    }
};

void lt_db_initialize() { }
void lt_db_finalize() { }
void lt_db_set_datadir( const char* /* dir */ ) { }

lt_tag_t* lt_tag_new()
{
    return new lt_tag_t;
}

lt_tag_t* lt_tag_copy(lt_tag_t *tag)
{
    return (tag ? new lt_tag_t( *tag) : nullptr);
}

void lt_tag_unref(lt_tag_t *tag)
{
    if (tag)
        tag->decRef();
}

/** See http://tools.ietf.org/html/rfc5646

    We are simply ignorant of grandfathered (irregular and regular) subtags and
    may either bail out or accept them, sorry (or not). However, we do accept
    any i-* irregular and x-* privateuse. Subtags are not checked for validity
    (alpha, digit, registered, ...).
 */
int lt_tag_parse(lt_tag_t *tag,
                 const char *tag_string,
                 lt_error_t **error)
{
    (void) error;
    if (!tag)
        return 0;
    tag->assign( tag_string);
    if (!tag_string)
        return 0;
    // In case we supported other subtags this would get more complicated.
    my_t_impl* aSubtags[] = { &tag->maLanguage, &tag->maScript, &tag->maRegion, &tag->maVariants, nullptr };
    my_t_impl** ppSub = &aSubtags[0];
    const char* pStart = tag_string;
    const char* p = pStart;
    const char* pEnd = pStart + strlen( pStart);   // scanning includes \0
    bool bStartLang = true;
    bool bPrivate = false;
    for ( ; p <= pEnd && ppSub && *ppSub; ++p)
    {
        if (p == pEnd || *p == '-')
        {
            size_t nLen = p - pStart;
            if (*ppSub == &tag->maLanguage)
            {
                if (bStartLang)
                {
                    bStartLang = false;
                    switch (nLen)
                    {
                        case 1:     // irregular or privateuse
                            if (*pStart == 'i' || *pStart == 'x')
                            {
                                (*ppSub)->assign( pStart, p);
                                bPrivate = true;
                                if (*pStart == 'x')
                                {
                                    // Simply copy all to privateuse field, we
                                    // do not care here what part actually is
                                    // private.
                                    tag->maPrivateUse.assign( pStart, pEnd);
                                }
                            }
                            else
                                return 0;   // bad
                            break;
                        case 2:     // ISO 639 alpha-2
                        case 3:     // ISO 639 alpha-3
                            (*ppSub)->assign( pStart, p);
                            break;
                        case 4:     // reserved for future use
                            return 0;   // bad
                            break;
                        case 5:
                        case 6:
                        case 7:
                        case 8:     // registered language subtag
                            (*ppSub++)->assign( pStart, p);
                            break;
                        default:
                            return 0;   // bad
                    }
                }
                else
                {
                    if (nLen > 8)
                        return 0;   // bad
                    if (bPrivate)
                    {
                        // Any combination of  "x" 1*("-" (2*8alphanum))
                        // allowed, store first as language and return ok.
                        // For i-* simply assume the same.
                        (*ppSub)->append( pStart-1, p);
                        return 1;  // ok
                    }
                    else if (nLen == 3)
                    {
                        // extlang subtag, 1 to 3 allowed we don't check that.
                        // But if it's numeric it's a region UN M.49 code
                        // instead and no script subtag is present, so advance.
                        if ('0' <= *pStart && *pStart <= '9')
                        {
                            ppSub += 2; // &tag->maRegion XXX watch this when inserting fields
                            --p;
                            continue;   // for
                        }
                        else
                            (*ppSub)->append( pStart-1, p);
                    }
                    else
                    {
                        // Not part of language subtag, advance.
                        ++ppSub;
                        --p;
                        continue;   // for
                    }
                }
            }
            else if (*ppSub == &tag->maScript)
            {
                switch (nLen)
                {
                    case 4:
                        // script subtag, or a (DIGIT 3alphanum) variant with
                        // no script and no region
                        if ('0' <= *pStart && *pStart <= '9')
                        {
                            ppSub += 2; // &tag->maVariants XXX watch this when inserting fields
                            --p;
                            continue;   // for
                        }
                        else
                            (*ppSub++)->assign( pStart, p);
                        break;
                    case 3:
                        // This may be a region UN M.49 code if 3DIGIT and no
                        // script code present. Just check first character and
                        // advance.
                        if ('0' <= *pStart && *pStart <= '9')
                        {
                            ++ppSub;
                            --p;
                            continue;   // for
                        }
                        else
                            return 0;   // bad
                        break;
                    case 2:
                        // script omitted, region subtag, advance.
                        ++ppSub;
                        --p;
                        continue;   // for
                        break;
                    case 1:
                        // script omitted, region omitted, extension subtag
                        // with singleton, stop parsing
                        ppSub = nullptr;
                        break;
                    case 5:
                    case 6:
                    case 7:
                    case 8:
                        // script omitted, region omitted, variant subtag
                        ppSub += 2; // &tag->maVariants XXX watch this when inserting fields
                        --p;
                        continue;   // for
                        break;
                    default:
                        return 0;   // bad
                }
            }
            else if (*ppSub == &tag->maRegion)
            {
                if (nLen == 2 || nLen == 3)
                    (*ppSub++)->assign( pStart, p);
                else
                {
                    // advance to variants
                    ++ppSub;
                    --p;
                    continue;   // for
                }
            }
            else if (*ppSub == &tag->maVariants)
            {
                // Stuff the remainder into variants, might not be correct, but ...
                switch (nLen)
                {
                    case 4:
                        // a (DIGIT 3alphanum) variant
                        if ('0' <= *pStart && *pStart <= '9')
                            ;   // nothing
                        else
                            return 0;   // bad
                        break;
                    case 5:
                    case 6:
                    case 7:
                    case 8:
                        ;   // nothing, variant
                        break;
                    default:
                        return 0;   // bad
                }
                (*ppSub)->append( pStart, p);
            }
            pStart = p+1;
        }
    }
    return 1;
}

char* lt_tag_canonicalize(lt_tag_t *tag,
                          lt_error_t **error)
{
    (void) error;
    return tag && tag->mpStr ? strdup( tag->mpStr) : nullptr;
}

const lt_lang_t* lt_tag_get_language(const lt_tag_t  *tag)
{
    return tag && tag->maLanguage.mpStr ? &tag->maLanguage : nullptr;
}

const lt_script_t *lt_tag_get_script(const lt_tag_t  *tag)
{
    return tag && tag->maScript.mpStr ? &tag->maScript : nullptr;
}

const lt_region_t *lt_tag_get_region(const lt_tag_t  *tag)
{
    return tag && tag->maRegion.mpStr ? &tag->maRegion : nullptr;
}

const lt_list_t *lt_tag_get_variants(const lt_tag_t  *tag)
{
    return tag ? tag->maVariants.mpList : nullptr;
}

const lt_string_t *lt_tag_get_privateuse(const lt_tag_t  *tag)
{
    return tag && tag->maPrivateUse.mpStr ? &tag->maPrivateUse : nullptr;
}

const char *lt_lang_get_tag(const lt_lang_t *lang)
{
    return lang ? lang->mpStr : nullptr;
}

const char *lt_script_get_tag(const lt_script_t *script)
{
    return script ? script->mpStr : nullptr;
}

const char *lt_region_get_tag(const lt_region_t *region)
{
    return region ? region->mpStr : nullptr;
}

const char *lt_variant_get_tag(const lt_variant_t *variant)
{
    return variant ? variant->mpStr : nullptr;
}

size_t lt_string_length(const lt_string_t *string)
{
    return string ? strlen(string->mpStr) : 0;
}

#ifdef erDEBUG
void lt_tag_dump(const lt_tag_t *tag)
{
    fprintf( stderr, "\n");
    fprintf( stderr, "SimpleLangtag  langtag: %s\n", tag->mpStr);
    fprintf( stderr, "SimpleLangtag language: %s\n", tag->maLanguage.mpStr);
    fprintf( stderr, "SimpleLangtag   script: %s\n", tag->maScript.mpStr);
    fprintf( stderr, "SimpleLangtag   region: %s\n", tag->maRegion.mpStr);
}
#endif

}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
