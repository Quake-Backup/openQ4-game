
#ifndef __LANGDICT_H__
#define __LANGDICT_H__

/*
===============================================================================

	Simple dictionary specifically for the localized string tables.

===============================================================================
*/

/*
===============================================================================

	Simple dictionary specifically for the localized string tables.

===============================================================================
*/

/*
===============================================================================

	The 8-bit codepage the string tables and the byte-indexed fonts share.

	Quake 4 draws text from 256 byte-indexed glyphs, so a string table byte and
	a font glyph have to agree on which character they mean. The stock tables
	are Windows-1252, which covers Western Europe but has no code point for
	a-ogonek, l-stroke or the other Polish letters. Windows-1250 is the
	Central European codepage the retail Polish release used, and it is what
	idStr's printable/upper/lower tables have always been built from.

===============================================================================
*/
typedef enum {
	LANGCP_WESTERN = 0,			// Windows-1252: English, French, Italian, Spanish, German
	LANGCP_CENTRAL_EUROPEAN		// Windows-1250: Polish
} langCodePage_t;

						// Which codepage a sys_lang value is authored in.
langCodePage_t			LangDict_CodePageForLanguage( const char *language );

						// Selected from sys_lang before the string tables load. The font
						// rasteriser reads it back so a byte means the same character in
						// the table and in the atlas.
void					LangDict_SetActiveCodePage( langCodePage_t codePage );
langCodePage_t			LangDict_GetActiveCodePage( void );

						// Bumped every time the active codepage actually changes, so the
						// font atlases can tell they are stale.
int						LangDict_GetCodePageGeneration( void );

						// The Unicode code point drawn for one string table byte. Returns 0
						// for the handful of slots the codepage leaves unassigned.
unsigned int			LangDict_UnicodeForByte( int byte );

class idLangKeyValue {
public:
	idStr					key;
	idStr					value;
};

class idLangDict {
public:
							idLangDict( void );
							~idLangDict( void );

	void					Clear( void );
	bool					Load( const char *fileName, bool clear = true );
	void					Save( const char *fileName );

	const char *			AddString( const char *str );
	const char *			GetString( const char *str ) const;

							// adds the value and key as passed (doesn't generate a "#str_xxxxxx" key or ensure the key/value pair is unique)
	void					AddKeyVal( const char *key, const char *val );

	int						GetNumKeyVals( void ) const;
	const idLangKeyValue *	GetKeyVal( int i ) const;

	void					SetBaseID(int id) { baseID = id; };

private:
	idList<idLangKeyValue>	args;
	idHashIndex				hash;

	bool					ExcludeString( const char *str ) const;
	int						GetNextId( void ) const;
	int						GetHashKey( const char *str ) const;

	int						baseID;
};

#endif /* !__LANGDICT_H__ */
