#include "stdafx.h"

SRCLINE(2158)
const float *__cdecl Material_RegisterLiteral(const float *literal)
{
	static DWORD dwCall = 0x0052CC60;

	__asm
	{
		mov edi, literal
		call [dwCall]
	}
}

SRCLINE(2197)
const char *Material_RegisterString(const char *string)
{
	return ((const char *(__cdecl *)(const char *))0x0052CD70)(string);
}

SRCLINE(2819)
bool Material_UsingTechnique(int techType)
{
	ASSERT(techType < ARRAYSIZE(g_useTechnique));

	return g_useTechnique[techType];
}

SRCLINE(2825)
bool Material_MatchToken(const char **text, const char *match)
{
	return Com_MatchToken(text, match, 1) != 0;
}

SRCLINE(3419)
bool Material_DefaultIndexRange(ShaderIndexRange *indexRangeRef, unsigned int arrayCount, ShaderIndexRange *indexRangeSet)
{
	if (arrayCount)
	{
		if (indexRangeRef->count + indexRangeRef->first > arrayCount)
			return false;
	}
	else if (indexRangeRef->first || indexRangeRef->count != 1)
		return false;

	indexRangeSet->first		= indexRangeRef->first;
	indexRangeSet->count		= indexRangeRef->count;
	indexRangeSet->isImplicit	= 0;
	return true;
}

SRCLINE(3437)
bool Material_ParseIndexRange(const char **text, unsigned int arrayCount, ShaderIndexRange *indexRange)
{
	if (*Com_Parse(text) != '[')
	{
		Com_UngetToken();
		indexRange->first = 0;
		indexRange->count = arrayCount;
		indexRange->isImplicit = 1;
		return true;
	}

	indexRange->isImplicit = 0;
	indexRange->first = Com_ParseInt(text);

	if (indexRange->first < arrayCount)
	{
		if (*Com_Parse(text) == ',')
		{
			int last = Com_ParseInt(text);

			if (last >= indexRange->first && last < arrayCount)
				return Material_MatchToken(text, "]");

			Com_ScriptError("ending index %i is not in the range [%i, %i]\n", last, indexRange->first, arrayCount - 1);
			return false;
		}
		else
		{
			Com_UngetToken();
			indexRange->count = 1;
			return true;
		}
	}

	Com_ScriptError("index %i is not in the range [0, %i]\n", indexRange->first, arrayCount - 1);
	return false;
}

SRCLINE(3487)
bool Material_ParseArrayOffset(const char **text, int arrayCount, int arrayStride, int *offset)
{
	if (!Material_MatchToken(text, "["))
		return false;

	int arrayIndex = Com_ParseInt(text);

	if (arrayIndex >= 0 && arrayIndex < arrayCount)
	{
		if (!Material_MatchToken(text, "]"))
			return false;

		*offset = arrayStride * arrayIndex;
		return true;
	}

	Com_ScriptError("array index must be in range [0, %i]\n", arrayCount - 1);
	return false;
}

SRCLINE(3506)
bool Material_CodeSamplerSource_r(const char **text, int offset, CodeSamplerSource *sourceTable, ShaderArgumentSource *argSource)
{
	ASSERT(text != nullptr);
	ASSERT(sourceTable != nullptr);

	if (!Material_MatchToken(text, "."))
		return false;

	const char *token = Com_Parse(text);

	int sourceIndex;
	for (sourceIndex = 0;; sourceIndex++)
	{
		if (!sourceTable[sourceIndex].name)
		{
			Com_ScriptError("unknown sampler source '%s'\n", token);
			return false;
		}

		if (!strcmp(token, sourceTable[sourceIndex].name))
			break;
	}

	if (sourceTable[sourceIndex].subtable)
	{
		if (sourceTable[sourceIndex].arrayCount)
		{
			int additionalOffset;

			if (!Material_ParseArrayOffset(
				text,
				sourceTable[sourceIndex].arrayCount,
				sourceTable[sourceIndex].arrayStride,
				&additionalOffset))
				return false;

			offset += additionalOffset;
		}

		return Material_CodeSamplerSource_r(text, offset, sourceTable[sourceIndex].subtable, argSource);
	}
	else
	{
		argSource->type = 4;
		argSource->u.codeIndex = offset + sourceTable[sourceIndex].source;

		if (sourceTable[sourceIndex].arrayCount)
		{
			ASSERT(sourceTable[sourceIndex].arrayStride == 1);
			return Material_ParseIndexRange(text, sourceTable[sourceIndex].arrayCount, &argSource->indexRange);
		}

		argSource->indexRange.first = 0;
		argSource->indexRange.count = 1;
		argSource->indexRange.isImplicit = true;
		return true;
	}

	return false;
}

SRCLINE(3555)
bool Material_ParseSamplerSource(const char **text, ShaderArgumentSource *argSource)
{
	const char *token = Com_Parse(text);

	if (!strcmp(token, "sampler"))
		return Material_CodeSamplerSource_r(text, 0, s_codeSamplers, argSource);

	if (!strcmp(token, "material"))
	{
		if (!Material_MatchToken(text, "."))
			return false;
		
		argSource->type = 2;
		argSource->u.literalConst = (const float *)Material_RegisterString(Com_Parse(text));
		argSource->indexRange.first = 0;
		argSource->indexRange.count = 1;
		argSource->indexRange.isImplicit = 1;

		return argSource->u.literalConst != nullptr;
	}

	Com_ScriptError("expected 'sampler' or 'material', found '%s' instead\n", token);
	return false;
}

SRCLINE(3580)
bool Material_DefaultSamplerSourceFromTable(const char *constantName, ShaderIndexRange *indexRange, CodeSamplerSource *sourceTable, ShaderArgumentSource *argSource)
{
	ASSERT(constantName != nullptr);
	ASSERT(sourceTable != nullptr);
	ASSERT(indexRange != nullptr);
	ASSERT(argSource != nullptr);

	for (int sourceIndex = 0; sourceTable[sourceIndex].name; sourceIndex++)
	{
		if (!sourceTable[sourceIndex].subtable
			&& !strcmp(constantName, sourceTable[sourceIndex].name)
			&& Material_DefaultIndexRange(indexRange, sourceTable[sourceIndex].arrayCount, &argSource->indexRange))
		{
			argSource->type			= 4;
			argSource->u.codeIndex	= LOWORD(sourceTable[sourceIndex].source);
			return true;
		}
	}

	return false;
}

SRCLINE(3606)
bool Material_DefaultSamplerSource(const char *constantName, ShaderIndexRange *indexRange, ShaderArgumentSource *argSource)
{
	return Material_DefaultSamplerSourceFromTable(constantName, indexRange, s_defaultCodeSamplers, argSource) != false;
}

SRCLINE(3613)
bool Material_ParseVector(const char **text, int elemCount, float *vector)
{
	if (Material_MatchToken(text, "("))
	{
		int elemIndex = 0;

		while (1)
		{
			vector[elemIndex++] = Com_ParseFloat(text);

			if (elemIndex == elemCount)
				break;

			if (!Material_MatchToken(text, ","))
				return false;
		}

		return Material_MatchToken(text, ")");
	}

	return false;
}

SRCLINE(3632)
bool Material_ParseLiteral(const char **text, const char *token, float *literal)
{
	ASSERT(text != nullptr);
	ASSERT(token != nullptr);
	ASSERT(literal != nullptr);

	literal[0] = 0.0f;
	literal[1] = 0.0f;
	literal[2] = 0.0f;
	literal[3] = 1.0f;

	if (!strcmp(token, "float1"))
		Material_ParseVector(text, 1, literal);
	else if (!strcmp(token, "float2"))
		Material_ParseVector(text, 2, literal);
	else if (!strcmp(token, "float3"))
		Material_ParseVector(text, 3, literal);
	else if (!strcmp(token, "float4"))
		Material_ParseVector(text, 4, literal);
	else
		return false;

	return true;
}

char Material_GetStreamDestForSemantic(_D3DXSEMANTIC *semantic)
{
	bool v2; // zf@8

	switch (semantic->Usage)
	{
	case 0:
		if (semantic->UsageIndex)
			goto LABEL_19;
		return 0;

	case 3:
		if (semantic->UsageIndex)
			goto LABEL_19;
		return 1;

	case 10:
		v2 = semantic->UsageIndex == 0;

		if (semantic->UsageIndex >= 2)
			goto LABEL_19;

		return semantic->UsageIndex + 2;

	case 12:
		if (semantic->UsageIndex)
			goto LABEL_19;
		return 4;

	case 5:
		v2 = semantic->UsageIndex == 0;

		if (semantic->UsageIndex >= 14)
			goto LABEL_19;

		return semantic->UsageIndex + 5;

	case 1:
		if (semantic->UsageIndex)
			goto LABEL_19;
		return 19;

	default:
	LABEL_19:
		ASSERT(false);
		//Com_Error(ERR_DROP, "Unknown shader input/output usage %i:%i\n", semantic->Usage, semantic->UsageIndex);
		return 0;
	}

	return 0;
}

SRCLINE(3657)
bool Material_ParseCodeConstantSource_r(MaterialShaderType shaderType, const char **text, int offset, CodeConstantSource *sourceTable, ShaderArgumentSource *argSource)
{
	ASSERT(text != nullptr);
	ASSERT(sourceTable != nullptr);
	ASSERT(argSource != nullptr);

	if (Material_MatchToken(text, "."))
	{
		const char *token = Com_Parse(text);

		int sourceIndex;
		for (sourceIndex = 0;; sourceIndex++)
		{
			if (!sourceTable[sourceIndex].name)
			{
				Com_ScriptError("unknown constant source '%s'\n", token);
				return false;
			}

			if (!strcmp(token, sourceTable[sourceIndex].name))
				break;
		}
		if (sourceTable[sourceIndex].arrayCount)
		{
			//ASSERT(sourceTable[sourceIndex].subtable || (sourceTable[sourceIndex].source < CONST_SRC_FIRST_CODE_MATRIX && sourceTable[sourceIndex].arrayStride == 1));

			if (sourceTable[sourceIndex].subtable)
			{
				int additionalOffset;

				if (!Material_ParseArrayOffset(
					text,
					sourceTable[sourceIndex].arrayCount,
					sourceTable[sourceIndex].arrayStride,
					&additionalOffset))
					return false;

				offset += additionalOffset;
			}
			else
			{
				ASSERT(sourceTable[sourceIndex].arrayStride == 1);

				if (!Material_ParseIndexRange(text, sourceTable[sourceIndex].arrayCount, &argSource->indexRange))
					return false;
			}
		}

		if (sourceTable[sourceIndex].subtable)
			return Material_ParseCodeConstantSource_r(shaderType, text, offset, sourceTable[sourceIndex].subtable, argSource);

		argSource->type = 2 * (shaderType != 0) + 3;
		argSource->u.codeIndex = offset + sourceTable[sourceIndex].source;

		//ASSERT((argSource->type == MTL_ARG_CODE_VERTEX_CONST) || s_codeConstUpdateFreq[argSource->u.codeIndex] != MTL_UPDATE_PER_PRIM);

		if (!sourceTable[sourceIndex].arrayCount)
		{
			if (argSource->u.codeIndex >= 197)
			{
				if (!Material_ParseIndexRange(text, 4, &argSource->indexRange))
					return false;
			}
			else
			{
				argSource->indexRange.first			= 0;
				argSource->indexRange.count			= 1;
				argSource->indexRange.isImplicit	= false;
			}
		}

		return true;
	}
	else
	{
		return false;
	}

	return false;
}

SRCLINE(3722)
bool Material_ParseConstantSource(MaterialShaderType shaderType, const char **text, ShaderArgumentSource *argSource)
{
	const char *token = Com_Parse(text);

	float literal[4];
	if (Material_ParseLiteral(text, token, literal))
	{
		argSource->type						= shaderType != MTL_VERTEX_SHADER ? 7 : 1;
		argSource->u.literalConst			= Material_RegisterLiteral(literal);
		argSource->indexRange.first			= 0;
		argSource->indexRange.count			= 1;
		argSource->indexRange.isImplicit	= true;

		return argSource->u.literalConst != nullptr;
	}

	if (!strcmp(token, "constant"))
		return Material_ParseCodeConstantSource_r(shaderType, text, 0, s_codeConsts, argSource);

	if (!strcmp(token, "material"))
	{
		if (!Material_MatchToken(text, "."))
			return false;

		token = Com_Parse(text);
		argSource->type						= shaderType != MTL_VERTEX_SHADER ? 6 : 0;
		argSource->u.literalConst			= (const float *)Material_RegisterString(token);
		argSource->indexRange.first			= 0;
		argSource->indexRange.count			= 1;
		argSource->indexRange.isImplicit	= true;

		return argSource->u.literalConst != nullptr;
	}

	Com_ScriptError("expected 'sampler' or 'material', found '%s' instead\n", token);
	return false;
}

SRCLINE(3758)
bool Material_DefaultConstantSourceFromTable(MaterialShaderType shaderType, const char *constantName, ShaderIndexRange *indexRange, CodeConstantSource *sourceTable, ShaderArgumentSource *argSource)
{
	int sourceIndex;
	for (sourceIndex = 0;; sourceIndex++)
	{
		if (!sourceTable[sourceIndex].name)
			return 0;

		if (!sourceTable[sourceIndex].subtable && !strcmp(constantName, sourceTable[sourceIndex].name))
		{
			unsigned int arrayCount;

			if (sourceTable[sourceIndex].source < 197)
			{
				int count	= sourceTable[sourceIndex].arrayCount > 1 ? sourceTable[sourceIndex].arrayCount : 1;
				arrayCount	= count;
			}
			else
			{
				ASSERT(sourceTable[sourceIndex].arrayCount == 0);
				arrayCount = 4;
			}

			if (Material_DefaultIndexRange(indexRange, arrayCount, &argSource->indexRange))
				break;
		}
	}

	argSource->type			= 2 * (shaderType != 0) + 3;
	argSource->u.codeIndex	= sourceTable[sourceIndex].source;

	//ASSERT(((argSource->type == MTL_ARG_CODE_VERTEX_CONST) || s_codeConstUpdateFreq[argSource->u.codeIndex] != MTL_UPDATE_PER_PRIM));
	return true;
}

SRCLINE(3791)
bool Material_DefaultConstantSource(MaterialShaderType shaderType, const char *constantName, ShaderIndexRange *indexRange, ShaderArgumentSource *argSource)
{
	if (Material_DefaultConstantSourceFromTable(shaderType, constantName, indexRange, s_codeConsts, argSource))
		return true;

	return Material_DefaultConstantSourceFromTable(shaderType, constantName, indexRange, s_defaultCodeConsts, argSource) != 0;
}

SRCLINE(3800)
bool Material_UnknownShaderworksConstantSource(MaterialShaderType shaderType, const char *constantName, ShaderIndexRange *indexRange, ShaderArgumentSource *argSource)
{
	static DWORD dwCall = 0x0052E910;

	__asm
	{
		mov ecx, shaderType
		mov eax, constantName
		push indexRange
		mov ebx, argSource
		call [dwCall]
		add esp, 0x4
	}
}

SRCLINE(3815)
unsigned int Material_ElemCountForParamName(const char *shaderName, ShaderUniformDef *paramTable, unsigned int paramCount, const char *name, ShaderParamType *paramType)
{
	unsigned int count = 0;

	for (unsigned int paramIndex = 0; paramIndex < paramCount; paramIndex++)
	{
		if (!strcmp(name, paramTable[paramIndex].name))
		{
			if (count && paramTable[paramIndex].type != *paramType)
				Com_Error(ERR_DROP, "param type changed from %i to %i", paramTable[paramIndex].type, *paramType);

			*paramType = paramTable[paramIndex].type;

			if (count <= paramTable[paramIndex].index)
				count = paramTable[paramIndex].index + 1;
			
			ASSERT(count > 0);
		}
	}

	return count;
}

SRCLINE(3840)
bool Material_ParseArgumentSource(MaterialShaderType shaderType, const char **text, const char *shaderName, ShaderParamType paramType, ShaderArgumentSource *argSource)
{
	ASSERT(text != nullptr);
	ASSERT(*text != '\0');
	ASSERT(shaderName != nullptr);
	ASSERT(argSource != nullptr);

	if (Material_MatchToken(text, "="))
	{
		if (paramType)
		{
			if (paramType > SHADER_PARAM_FLOAT4 && paramType <= SHADER_PARAM_SAMPLER_1D)
				return Material_ParseSamplerSource(text, argSource);
		
			ASSERT(false && "Unknown constant type");
			return false;
		}

		return Material_ParseConstantSource(shaderType, text, argSource);
	}

	return false;
}

SRCLINE(3868)
bool Material_DefaultArgumentSource(MaterialShaderType shaderType, const char *constantName, ShaderParamType paramType, ShaderIndexRange *indexRange, ShaderArgumentSource *argSource)
{
	ASSERT(constantName != nullptr);
	ASSERT(argSource != nullptr);

	printf("type: %d name: %s param: %d\n", shaderType, constantName, paramType);

	if (paramType)
	{
		if (paramType > SHADER_PARAM_FLOAT4 && paramType <= SHADER_PARAM_SAMPLER_1D)
			return Material_DefaultSamplerSource(constantName, indexRange, argSource);

		return false;
	}

	if (Material_DefaultConstantSource(shaderType, constantName, indexRange, argSource))
		return true;

	return Material_UnknownShaderworksConstantSource(shaderType, constantName, indexRange, argSource);
}

SRCLINE(3980)
int Material_CompareShaderArgumentsForCombining(const void *e0, const void *e1)
{
	MaterialShaderArgument *c1 = (MaterialShaderArgument *)e0;
	MaterialShaderArgument *c2 = (MaterialShaderArgument *)e1;

	int v4 = c1->type == 4 || c1->type == 2;
	int v3 = c2->type == 4 || c2->type == 2;

	if (v4 == v3)
		return c1->dest - c2->dest;
  
	return v4 - v3;
}

SRCLINE(3994)
bool Material_AttemptCombineShaderArguments(MaterialShaderArgument *arg0, MaterialShaderArgument *arg1)
{
	if (arg0->type != arg1->type)
		return false;

	if (arg0->type != 3 && arg0->type != 5)
		return false;

	if (arg0->u.codeConst.rowCount + arg0->dest != arg1->dest)
		return false;

	if ((signed int)LOWORD(arg0->u.literalConst) < 197)
		return false;

	if (arg0->u.codeConst.index != arg1->u.codeConst.index)
		return false;
						
	if (arg0->u.codeConst.rowCount + arg0->u.codeConst.firstRow != arg1->u.codeConst.firstRow)
		return false;

	ASSERT((arg1->u.codeConst.rowCount + arg0->u.codeConst.rowCount + arg0->u.codeConst.firstRow < 2
		|| arg1->u.codeConst.rowCount + arg0->u.codeConst.rowCount + arg0->u.codeConst.firstRow > 4));

	arg0->u.codeConst.rowCount += arg1->u.codeConst.rowCount;
	return true;
}

SRCLINE(4020)
unsigned int Material_CombineShaderArguments(unsigned int usedCount, MaterialShaderArgument *localArgs)
{
	int dstIndex = 0;

	for (unsigned int srcIndex = 1; srcIndex < usedCount; srcIndex++)
	{
		if (!Material_AttemptCombineShaderArguments(&localArgs[dstIndex], &localArgs[srcIndex]))
		{
			dstIndex++;
			
			localArgs[dstIndex].type			= localArgs[srcIndex].type;
			localArgs[dstIndex].dest			= localArgs[srcIndex].dest;
			localArgs[dstIndex].u.codeSampler	= localArgs[srcIndex].u.codeSampler;
		}
	}

	return dstIndex + 1;
}

SRCLINE(4037)
bool Material_SetShaderArguments(unsigned int usedCount, MaterialShaderArgument *localArgs, unsigned int argLimit, unsigned int *argCount, MaterialShaderArgument *args)
{
	ASSERT(args != nullptr);
	ASSERT(argCount != nullptr);

	if (usedCount)
	{
		if (*argCount + usedCount > argLimit)
		{
			Com_ScriptError("more than %i total shader arguments\n", argLimit);
			return false;
		}
		
		// Sort arguments so they can be easily combined
		qsort(localArgs, usedCount, sizeof(MaterialShaderArgument), Material_CompareShaderArgumentsForCombining);

		// Combine the arguments if possible (and get the new count)
		usedCount = Material_CombineShaderArguments(usedCount, localArgs);

		// Copy all of the fixed arguments to the real table
		memcpy((char *)&args[*argCount], (char *)localArgs, sizeof(MaterialShaderArgument) * usedCount);

		// Adjust the output count
		*argCount += usedCount;
	}

	return true;
}

SRCLINE(4058)
ShaderUniformDef *Material_GetShaderArgumentDest(const char *paramName, unsigned int paramIndex, ShaderUniformDef *paramTable, unsigned int paramCount)
{
	for (unsigned int tableIndex = 0; tableIndex < paramCount; tableIndex++)
	{
		if (paramTable[tableIndex].index == paramIndex && !strcmp(paramTable[tableIndex].name, paramName))
		{
			if (paramTable[tableIndex].isAssigned)
			{
				Com_ScriptError("parameter %s index %i already assigned\n", paramName, paramIndex);
				return nullptr;
			}

			paramTable[tableIndex].isAssigned = true;
			return &paramTable[tableIndex];
		}
	}

	ASSERT(false && "unfound name should be caught earlier");
	return nullptr;
}

SRCLINE(4083)
bool MaterialAddShaderArgument(const char *shaderName, const char *paramName, MaterialShaderArgument *arg, char (*registerUsage)[64])
{
	if (arg->type > 1 && arg->type != 3)
		return true;

	if (arg->dest >= R_MAX_PIXEL_SHADER_CONSTS)
	{
		Com_ScriptError("Invalid vertex register index %d in '%s' for '%s'\n", arg->dest, shaderName, paramName);
		return false;
	}

	if ((*registerUsage)[64 * arg->dest])
	{
		Com_ScriptError(
			"Vertex register collision at index %d in '%s' between '%s' and '%s'\n",
			arg->dest,
			shaderName,
			&(*registerUsage)[64 * arg->dest],
			paramName);

		return false;
	}

	I_strncpyz(&(*registerUsage)[64 * arg->dest], paramName, 64);
	return true;
}

SRCLINE(4112)
bool Material_AddShaderArgumentFromLiteral(const char *shaderName, const char *paramName, unsigned __int16 type, const float *literal, ShaderUniformDef *dest, MaterialShaderArgument *arg, char(*registerUsage)[64])
{
	ASSERT(type != 7 && arg->dest < R_MAX_PIXEL_SHADER_CONSTS);

	arg->type			= type;
	arg->dest			= dest->resourceDest;
	arg->u.codeSampler	= (unsigned int)literal;

	return MaterialAddShaderArgument(shaderName, paramName, arg, registerUsage);
}

SRCLINE(4129)
bool Material_AddShaderArgumentFromCodeConst(const char *shaderName, const char *paramName, unsigned __int16 type, unsigned int codeIndex, unsigned int offset, ShaderUniformDef *dest, MaterialShaderArgument *arg, char(*registerUsage)[64])
{
	ASSERT(type != 5 && arg->dest < R_MAX_PIXEL_SHADER_CONSTS);

	arg->type					= type;
	arg->dest					= dest->resourceDest;
	arg->u.codeConst.rowCount	= 1;

	if (codeIndex < 197)
	{
		arg->u.codeConst.index		= offset + codeIndex;
		arg->u.codeConst.firstRow	= 0;
	}
	else
	{
		if (dest->isTransposed)
			arg->u.codeConst.index = ((codeIndex - 197) ^ 2) + 197;
		else
			arg->u.codeConst.index = codeIndex;

		arg->u.codeConst.firstRow = offset;
	}

	return MaterialAddShaderArgument(shaderName, paramName, arg, registerUsage);
}

SRCLINE(4159)
void Material_AddShaderArgumentFromCodeSampler(unsigned __int16 type, unsigned int codeSampler, ShaderUniformDef *dest, MaterialShaderArgument *arg)
{
	arg->type			= type;
	arg->dest			= dest->resourceDest;
	arg->u.codeSampler	= codeSampler;
}

SRCLINE(4166)
bool Material_AddShaderArgumentFromMaterial(const char *shaderName, const char *paramName, unsigned __int16 type, const char *name, ShaderUniformDef *dest, MaterialShaderArgument *arg, char(*registerUsage)[64])
{
	ASSERT(type != 6 && arg->dest < R_MAX_PIXEL_SHADER_CONSTS);

	Material_RegisterString(name);

	arg->type			= type;
	arg->dest			= dest->resourceDest;
	arg->u.codeSampler	= R_HashString(name);

	return MaterialAddShaderArgument(shaderName, paramName, arg, registerUsage);
}

SRCLINE(4185)
bool Material_AddShaderArgument(const char *shaderName, ShaderArgumentSource *argSource, ShaderArgumentDest *argDest, ShaderUniformDef *paramTable, unsigned int paramCount, unsigned int *usedCount, MaterialShaderArgument *argTable, char(*registerUsage)[64])
{
	if (argSource->indexRange.isImplicit)
	{
		ASSERT(argSource->indexRange.first == 0);

		if (argDest->indexRange.count > argSource->indexRange.count)
		{
			Com_ScriptError("The destination needs %i entries, but the source can only provide %i",
				argDest->indexRange.count,
				argSource->indexRange.count);

			return false;
		}

		argSource->indexRange.count = argDest->indexRange.count;
	}
	else if (argDest->indexRange.count != argSource->indexRange.count)
	{
		Com_ScriptError("The destination needs %i entries, but the source provides %i",
			argDest->indexRange.count,
			argSource->indexRange.count);

		return false;
	}

	switch (argSource->type)
	{
	case 1:
	case 7:
	{
		if (argDest->indexRange.count != 1)
		{
			Com_ScriptError("Must assign literals to a constant one row at a time");
			return false;
		}

		ShaderUniformDef *dest = Material_GetShaderArgumentDest(
			argDest->paramName,
			argDest->indexRange.first,
			paramTable,
			paramCount);

		if (!dest)
			return false;

		if (!Material_AddShaderArgumentFromLiteral(
			shaderName,
			argDest->paramName,
			argSource->type,
			argSource->u.literalConst,
			dest,
			&argTable[*usedCount],
			registerUsage))
			return false;

		*usedCount += 1;
		return true;
	}

	case 3:
	case 5:
	{
		for (unsigned int indexOffset = 0; indexOffset < argDest->indexRange.count; ++indexOffset)
		{
			ShaderUniformDef *dest = Material_GetShaderArgumentDest(
				argDest->paramName,
				indexOffset + argDest->indexRange.first,
				paramTable,
				paramCount);

			if (!dest)
				return false;

			if (!Material_AddShaderArgumentFromCodeConst(
				shaderName,
				argDest->paramName,
				argSource->type,
				LOWORD(argSource->u.literalConst),
				indexOffset + argSource->indexRange.first,
				dest,
				&argTable[*usedCount],
				registerUsage))
				return false;

			*usedCount += 1;
		}

		return true;
	}

	case 4:
	{
		for (unsigned int indexOffset = 0; indexOffset < argDest->indexRange.count; indexOffset++)
		{
			ShaderUniformDef *dest = Material_GetShaderArgumentDest(
				argDest->paramName,
				indexOffset + argDest->indexRange.first,
				paramTable,
				paramCount);

			if (!dest)
				return false;

			Material_AddShaderArgumentFromCodeSampler(
				argSource->type,
				indexOffset + argSource->indexRange.first + argSource->u.codeIndex,
				dest,
				&argTable[*usedCount]);

			*usedCount += 1;
		}

		return true;
	}

	case 0:
	case 2:
	case 6:
	{
		if (argDest->indexRange.count != 1)
		{
			Com_ScriptError("Must assign material values one at a time");
			return false;
		}

		ASSERT(argSource->indexRange.first == 0);
		ASSERT(argSource->indexRange.count == 1);

		ShaderUniformDef *dest = Material_GetShaderArgumentDest(
			argDest->paramName,
			argDest->indexRange.first,
			paramTable,
			paramCount);

		if (!dest)
			return false;

		if (!Material_AddShaderArgumentFromMaterial(
			shaderName,
			argDest->paramName,
			argSource->type,
			argSource->u.name,
			dest,
			&argTable[*usedCount],
			registerUsage))
			return false;

		*usedCount += 1;
		return true;
	}

	default:
		ASSERT(false && "Unhandled case");
		break;
	}

	return false;
}

SRCLINE(4275)
bool CodeConstIsOneOf(unsigned __int16 constCodeIndex, const unsigned __int16 *consts, int numConsts)
{
	for (int constIdx = 0; constIdx < numConsts; constIdx++)
	{
		if (consts[constIdx] == constCodeIndex)
			return true;
	}

	return false;
}

SRCLINE(4285)
bool Material_ParseShaderArguments(const char **text, const char *shaderName, MaterialShaderType shaderType, ShaderUniformDef *paramTable, unsigned int paramCount, unsigned __int16 *techFlags, unsigned int argLimit, unsigned int *argCount, MaterialShaderArgument *args)
{
	unsigned __int16 v10; // cx@54
	__int16 v14; // [sp+18h] [bp-5134h]@24
	unsigned __int16 v15; // [sp+1Ch] [bp-5130h]@25

	ASSERT(techFlags != nullptr);
	ASSERT(paramTable  != nullptr);

	if (!Material_MatchToken(text, "{"))
		return false;

	//
	// Values for shader registers stored as 64-byte strings
	//
	char registerUsage[16384];
	memset(registerUsage, 0, sizeof(registerUsage));

	//
	// Array of parsed shader arguments
	//
	MaterialShaderArgument localArgs[512];

	//
	// Current shader argument dest/source
	//
	ShaderArgumentDest argDest;
	ShaderArgumentSource argSource;

	unsigned int usedCount = 0;
	while (1)
	{
		const char *token = Com_Parse(text);

		if (!*token)
		{
			Com_ScriptError("unexpected end-of-file\n");
			return false;
		}

		if (*token == '}')
			break;

		char paramName[256];
		I_strncpyz(paramName, token, sizeof(paramName));

		ShaderParamType paramType;
		unsigned int registerCount = Material_ElemCountForParamName(shaderName, paramTable, paramCount, paramName, &paramType);

		if (registerCount)
		{
			if (!Material_ParseIndexRange(text, registerCount, &argDest.indexRange))
				return false;

			argDest.paramName = paramName;
			
			if (!Material_ParseArgumentSource(shaderType, text, shaderName, paramType, &argSource))
				return false;
			
			if (!Material_MatchToken(text, ";"))
				return false;
			
			if (!Material_AddShaderArgument(
				shaderName,
				&argSource,
				&argDest,
				paramTable,
				paramCount,
				&usedCount,
				localArgs,
				(char(*)[64])registerUsage))
				return false;

			if (v14 == 4)
			{
				switch (v15)
				{
				case 9:
					*techFlags |= 1;
					break;
				case 10:
					*techFlags |= 2;
					break;
				case 19:
				case 20:
				case 21:
					*techFlags |= 64;
					break;
				}
			}
		}
		else
		{
			if (!Material_MatchToken(text, "="))
				return false;

			Com_SkipRestOfLine(text);
		}
	}

	if (usedCount == paramCount)
		return Material_SetShaderArguments(usedCount, localArgs, argLimit, argCount, args);

	//
	// Try and use default arguments for undefined parameters
	//
	for (unsigned int paramIndex = 0; paramIndex < paramCount; paramIndex++)
	{
		if (paramTable[paramIndex].isAssigned)
			continue;

		argDest.paramName				= paramTable[paramIndex].name;
		argDest.indexRange.first		= paramTable[paramIndex].index;
		argDest.indexRange.count		= 1;
		argDest.indexRange.isImplicit	= false;

		if (Material_DefaultArgumentSource(
			shaderType,
			paramTable[paramIndex].name,
			paramTable[paramIndex].type,
			&argDest.indexRange,
			&argSource))
		{
			if (v14 == 5)
			{
				if (v15 == 4)
					*techFlags |= 0x10u;
			}
			else if (v14 == 4 && (v15 == 19 || v15 == 20 || v15 == 21))
			{
				*techFlags |= 0x40u;
			}

			if (v14 == 3 && v15 == 120)
				*techFlags |= 0x100u;
			
			if (v14 == 3)
			{
				v10 = *techFlags;
				if (!(v10 & 0x20))
				{
					unsigned short foliageConsts[4];
					foliageConsts[0] = 81;
					foliageConsts[1] = 82;
					foliageConsts[2] = 83;
					foliageConsts[3] = 84;

					if (CodeConstIsOneOf(v15, (const unsigned __int16 *)foliageConsts, 4))
						*techFlags |= 0x20u;
				}
			}

			if (v14 == 3 && !(*techFlags & 0x200))
			{
				unsigned short scorchConsts[2];
				scorchConsts[0] = 114;
				scorchConsts[1] = 118;

				if (CodeConstIsOneOf(v15, scorchConsts, 2))
					*techFlags |= 0x200u;
			}

			if (!Material_AddShaderArgument(
				shaderName,
				&argSource,
				&argDest,
				paramTable,
				paramCount,
				&usedCount,
				localArgs,
				(char(*)[64])registerUsage))
				return false;
		}
	}

	//
	// Apply arguments if all parameters matched
	//
	if (usedCount == paramCount)
		return Material_SetShaderArguments(usedCount, localArgs, argLimit, argCount, args);

	//
	// Notify the user of any undefined arguments in the shader
	//
	Com_PrintWarning(8, "Undefined shader parameter(s) in %s\n", shaderName);

	for (unsigned int paramIndex = 0; paramIndex < paramCount; paramIndex++)
	{
		if (!paramTable[paramIndex].isAssigned)
			Com_PrintWarning(8, "  %s\n", paramTable[paramIndex].name);
	}

	Com_PrintWarning(8, "%i parameter(s) were undefined\n", paramCount - usedCount);
	return false;
}

SRCLINE(4720)
bool Material_CopyTextToDXBuffer(void *cachedShader, unsigned int shaderLen, void **shader)
{
	static DWORD dwCall = 0x0052F6B0;

	__asm
	{
		mov esi, shaderLen
		mov edi, shader
		mov ebx, cachedShader
		call [dwCall]
	}
}

FILE *Material_OpenShader_BlackOps(const char *shaderName, const char *shaderVersion)
{
	//
	// Determine if this was a vertex shader or pixel shader
	//
	const char *shaderMain;

	if (shaderVersion[0] == 'v' && shaderVersion[1] == 's')
		shaderMain = "vs_main";
	else
		shaderMain = "ps_main";

	//
	// Load the shader directly from the name
	//
	char shaderPath[MAX_PATH];
	Com_sprintf(shaderPath, MAX_PATH, "%s\\raw\\shadercache_mods\\%s_%s_%s",
		*(DWORD *)(*(DWORD *)0x258E9F0 + 12),
		shaderMain,
		shaderVersion,
		shaderName);

	return fopen(shaderPath, "rb");
}

FILE *Material_OpenShader_WAW(const char *shaderName, const char *shaderVersion)
{
	//
	// Create a unique shader string to hash
	//
	char shaderString[MAX_PATH];
	Com_sprintf(shaderString, MAX_PATH, "%s_%s", shaderVersion, shaderName);

	//
	// Determine the path to load the shader from
	//
	char shaderPath[MAX_PATH];
	Com_sprintf(shaderPath, MAX_PATH, "%s\\raw\\shader_bin\\%s_%8.8x",
		*(DWORD *)(*(DWORD *)0x258E9F0 + 12),
		shaderVersion,
		R_HashAssetName(shaderString));

	return fopen(shaderPath, "rb");
}

SRCLINE(4948)
void *Material_LoadShader(const char *shaderName, const char *shaderVersion)
{
	//
	// Try loading the black ops version first
	//
	int shaderDataSize	= 0;
	FILE *shaderFile	= Material_OpenShader_BlackOps(shaderName, shaderVersion);
	
	if (shaderFile)
	{
		//
		// Skip the first 4 bytes (zeros)
		//
		fpos_t pos = 4;
		fsetpos(shaderFile, &pos);

		//
		// Read the real data size
		//
		if (fread(&shaderDataSize, 4, 1, shaderFile) < 1)
		{
			fclose(shaderFile);
			return 0;
		}
	}
	else
	{
		//
		// Load the WAW version if it wasn't found
		//
		shaderFile = Material_OpenShader_WAW(shaderName, shaderVersion);

		if (!shaderFile)
			return 0;

		if (fread(&shaderDataSize, 4, 1, shaderFile) < 1)
		{
			fclose(shaderFile);
			return 0;
		}
	}

	void *shaderMemory	= Z_Malloc(shaderDataSize);
	void *shader		= nullptr;

	fread(shaderMemory, 1, shaderDataSize, shaderFile);

	if (!Material_CopyTextToDXBuffer(shaderMemory, shaderDataSize, &shader))
		ASSERT(false && "SHADER CREATION FAILED\n");

	fclose(shaderFile);
	Z_Free(shaderMemory);
	return shader;
}

SRCLINE(8710)
void *Material_RegisterTechnique(const char *name, int renderer)
{
	static DWORD dwCall = 0x00530A40;

	__asm
	{
		mov esi, renderer
		mov eax, name
		call [dwCall]
	}
}

SRCLINE(8727)
bool Material_IgnoreTechnique(const char *name)
{
	const char *techniqueNames[1];
	techniqueNames[0] = "\"none\"";

	for (int techniqueIndex = 0; techniqueIndex < 1; techniqueIndex++)
	{
		if (!strcmp(name, techniqueNames[techniqueIndex]))
			return true;
	}

	return false;
}

SRCLINE(8763)
int Material_TechniqueTypeForName(const char *name)
{
	const char *techniqueNames[59];
	techniqueNames[0] = "\"depth prepass\"";
	techniqueNames[1] = "\"build floatz\"";
	techniqueNames[2] = "\"build shadowmap depth\"";
	techniqueNames[3] = "\"build shadowmap color\"";
	techniqueNames[4] = "\"unlit\"";
	techniqueNames[5] = "\"emissive\"";
	techniqueNames[6] = "\"emissive shadow\"";
	techniqueNames[7] = "\"emissive reflected\"";
	techniqueNames[8] = "\"lit\"";
	techniqueNames[9] = "\"lit fade\"";
	techniqueNames[10] = "\"lit sun\"";
	techniqueNames[11] = "\"lit sun fade\"";
	techniqueNames[12] = "\"lit sun shadow\"";
	techniqueNames[13] = "\"lit sun shadow fade\"";
	techniqueNames[14] = "\"lit spot\"";
	techniqueNames[15] = "\"lit spot fade\"";
	techniqueNames[16] = "\"lit spot shadow\"";
	techniqueNames[17] = "\"lit spot shadow fade\"";
	techniqueNames[18] = "\"lit omni\"";
	techniqueNames[19] = "\"lit omni fade\"";
	techniqueNames[20] = "\"lit omni shadow\"";
	techniqueNames[21] = "\"lit omni shadow fade\"";
	techniqueNames[22] = "\"lit charred\"";
	techniqueNames[23] = "\"lit fade charred\"";
	techniqueNames[24] = "\"lit sun charred\"";
	techniqueNames[25] = "\"lit sun fade charred\"";
	techniqueNames[26] = "\"lit sun shadow charred\"";
	techniqueNames[27] = "\"lit sun shadow fade charred\"";
	techniqueNames[28] = "\"lit spot charred\"";
	techniqueNames[29] = "\"lit spot fade charred\"";
	techniqueNames[30] = "\"lit spot shadow charred\"";
	techniqueNames[31] = "\"lit spot shadow fade charred\"";
	techniqueNames[32] = "\"lit omni charred\"";
	techniqueNames[33] = "\"lit omni fade charred\"";
	techniqueNames[34] = "\"lit omni shadow charred\"";
	techniqueNames[35] = "\"lit omni shadow fade charred\"";
	techniqueNames[36] = "\"lit instanced\"";
	techniqueNames[37] = "\"lit instanced sun\"";
	techniqueNames[38] = "\"lit instanced sun shadow\"";
	techniqueNames[39] = "\"lit instanced spot\"";
	techniqueNames[40] = "\"lit instanced spot shadow\"";
	techniqueNames[41] = "\"lit instanced omni\"";
	techniqueNames[42] = "\"lit instanced omni shadow\"";
	techniqueNames[43] = "\"light spot\"";
	techniqueNames[44] = "\"light omni\"";
	techniqueNames[45] = "\"light spot shadow\"";
	techniqueNames[46] = "\"light spot charred\"";
	techniqueNames[47] = "\"light omni charred\"";
	techniqueNames[48] = "\"light spot shadow charred\"";
	techniqueNames[49] = "\"fakelight normal\"";
	techniqueNames[50] = "\"fakelight view\"";
	techniqueNames[51] = "\"sunlight preview\"";
	techniqueNames[52] = "\"case texture\"";
	techniqueNames[53] = "\"solid wireframe\"";
	techniqueNames[54] = "\"shaded wireframe\"";
	techniqueNames[55] = "\"shadowcookie caster\"";
	techniqueNames[56] = "\"shadowcookie receiver\"";
	techniqueNames[57] = "\"debug bumpmap\"";
	techniqueNames[58] = "\"debug bumpmap instanced\"";

	for (int techniqueIndex = 0; techniqueIndex < 59; techniqueIndex++)
	{
		if (!strcmp(name, techniqueNames[techniqueIndex]))
			return techniqueIndex;
	}

	return 59;
}

SRCLINE(9023)
void *__cdecl Material_LoadTechniqueSet(const char *name, int renderer)
{
	char techType[130];

	//
	// Create a file path using normal techsets and read data
	//
	char filename[MAX_PATH];
	Com_sprintf(filename, MAX_PATH, "pimp/techsets/%s.techset", name);

	void *fileData;
	int fileSize = FS_ReadFile(filename, (void **)&fileData);

	if (fileSize < 0)
	{
		//
		// Try loading with PIMP enabled
		//
		//Com_sprintf(filename, MAX_PATH, "techsets/%s.techset", name);
		//fileSize = FS_ReadFile(filename, (void **)&fileData);

		if (fileSize < 0)
		{
			Com_PrintError(8, "^1ERROR: Couldn't open techniqueSet '%s'\n", filename);
			return nullptr;
		}
	}

	//
	// Allocate the techset structure
	//
	const char *textData	= (const char *)fileData;
	size_t nameSize			= strlen(name) + 1;
	char *techniqueSet		= (char *)Z_Malloc(nameSize + 248);

	*(char **)(techniqueSet + 0)	= techniqueSet + 248;
	*(BYTE *)(techniqueSet + 4)		= 0;
	*(char **)(techniqueSet + 8)	= techniqueSet;

	memcpy(techniqueSet + 248, name, nameSize);

	//
	// TODO: What does this function actually do?
	//
	((void(__cdecl *)())0x005525D0)();

	//
	// Begin the text parsing session
	//
	Com_BeginParseSession(filename);
	Com_SetScriptWarningPrefix("^1ERROR: ");
	Com_SetSpaceDelimited(0);
	Com_SetKeepStringQuotes(1);

	int techTypeCount	= 0;
	bool usingTechnique = false;
	while (1)
	{
		const char *token = Com_Parse(&textData);

		if (*token == '\0')
			break;

		if (*token == '"')
		{
			if (techTypeCount == 59)
			{
				Com_ScriptError("Too many labels in technique set\n");
				techniqueSet = 0;
				break;
			}

			if (!Material_IgnoreTechnique(token))
			{
				techType[techTypeCount] = Material_TechniqueTypeForName(token);

				if (techType[techTypeCount] == 59)
				{
					Com_ScriptError("Unknown technique type '%s'\n", token);
					techniqueSet = 0;
					break;
				}

				if (Material_UsingTechnique(techType[techTypeCount]))
					usingTechnique = true;

				++techTypeCount;
			}

			if (!Material_MatchToken(&textData, ":"))
			{
				techniqueSet = 0;
				break;
			}
		}
		else
		{
			if (usingTechnique)
			{
				if (!techTypeCount)
				{
					Com_ScriptError("Unknown technique type '%s'\n", token);
					techniqueSet = 0;
					break;
				}

				void *technique = Material_RegisterTechnique(token, renderer);
				if (!technique)
				{
					Com_ScriptError("Couldn't register technique '%s'\n", token);
					techniqueSet = 0;
					break;
				}

				for (int techTypeIndex = 0; techTypeIndex < techTypeCount; ++techTypeIndex)
					*(DWORD *)&techniqueSet[4 * techType[techTypeIndex] + 12] = (DWORD)technique;
			}

			techTypeCount	= 0;
			usingTechnique	= false;
			if (!Material_MatchToken(&textData, ";"))
			{
				techniqueSet = 0;
				break;
			}
		}
	}

	Com_EndParseSession();
	FS_FreeFile(fileData);
	return techniqueSet;
}

bool __declspec(naked) hk_Material_AddShaderArgument()
{
	__asm
	{
		push ebp
		mov ebp, esp

		push [ebp + 0x20]					// a8: registerUsage
		push [ebp + 0x1C]					// a7: argTable
		push [ebp + 0x18]					// a6: usedCount
		push [ebp + 0x14]					// a5: paramCount
		push [ebp + 0x10]					// a4: paramTable
		push eax							// a3: argDest
		push [ebp + 0x0C]					// a2: argSource
		push [ebp + 0x08]					// a1: shaderName
		call Material_AddShaderArgument
		add esp, 0x20

		pop ebp
		retn
	}
}

char __declspec(naked) hk_Material_GetStreamDestForSemantic()
{
	__asm
	{
		push ebp
		mov ebp, esp

		push eax
		call Material_GetStreamDestForSemantic
		add esp, 0x4

		pop ebp
		retn
	}
}

bool __declspec(naked) hk_Material_ParseSamplerSource()
{
	__asm
	{
		push ebp
		mov ebp, esp

		push [ebp + 0x8]
		push ebx
		call Material_ParseSamplerSource
		add esp, 0x8

		pop ebp
		retn
	}
}

bool __declspec(naked) hk_Material_ParseConstantSource()
{
	__asm
	{
		push ebp
		mov ebp, esp

		push ebx
		push [ebp + 0xC]
		push [ebp + 0x8]
		call Material_ParseConstantSource
		add esp, 0xC

		pop ebp
		retn
	}
}

bool __declspec(naked) hk_Material_DefaultArgumentSource()
{
	__asm
	{
		push ebp
		mov ebp, esp

		push ecx
		push esi
		push eax
		push edi
		push [ebp + 0x8]
		call Material_DefaultArgumentSource
		add esp, 0x14

		pop ebp
		retn
	}
}

void __declspec(naked) hk_Material_LoadShader()
{
	__asm
	{
		push ebp
		mov ebp, esp

		push ecx
		push [ebp + 0x8]
		call Material_LoadShader
		add esp, 0x8

		pop ebp
		retn
	}
}

const MaterialUpdateFrequency s_codeConstUpdateFreq[197] =
{
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 1, 1, 1, 1, 1, 1, 0, 1,
	0, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	0, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
};

const bool g_useTechnique[130] =
{
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 0, 0, 0, 0, 1, 0, 1, 1, 1,
};

CodeSamplerSource s_lightmapSamplers[] =
{
	{ "primary", 4, 0, 0, 0 },
	{ "secondary", 5, 0, 0, 0 },
	{ "secondaryb", 33, 0, 0, 0 },

	{ nullptr, 0, 0, 0, 0 },
};

CodeSamplerSource s_lightSamplers[] =
{
	{ "attenuation", 16, 0, 0, 0 },

	{ nullptr, 0, 0, 0, 0 },
};

CodeSamplerSource s_codeSamplers[] =
{
	{ "white", 1, 0, 0, 0 },
	{ "black", 0, 0, 0, 0 },
	{ "identityNormalMap", 2, 0, 0, 0 },
	{ "lightmap", 4, s_lightmapSamplers, 0, 0 },
	{ "outdoor", 18, 0, 0, 0 },
	{ "shadowmapSun", 6, 0, 0, 0 },
	{ "shadowmapSpot", 7, 0, 0, 0 },
	{ "feedback", 8, 0, 0, 0 },
	{ "resolvedPostSun", 9, 0, 0, 0 },
	{ "resolvedScene", 10, 0, 0, 0 },
	{ "postEffectSrc", 11, 0, 0, 0 },
	{ "postEffectGodRays", 12, 0, 0, 0 },
	{ "postEffect0", 13, 0, 0, 0 },
	{ "postEffect1", 14, 0, 0, 0 },
	{ "sky", 15, 0, 0, 0 },
	{ "light", 16, s_lightSamplers, 0, 0 },
	{ "floatZ", 19, 0, 0, 0 },
	{ "processedFloatZ", 20, 0, 0, 0 },
	{ "rawFloatZ", 21, 0, 0, 0 },
	{ "caseTexture", 22, 0, 0, 0 },
	{ "codeTexture0", 34, 0, 0, 0 },
	{ "codeTexture1", 35, 0, 0, 0 },
	{ "codeTexture2", 36, 0, 0, 0 },
	{ "codeTexture3", 37, 0, 0, 0 },
	{ "impactMask", 38, 0, 0, 0 },
	{ "ui3d", 39, 0, 0, 0 },
	{ "missileCam", 40, 0, 0, 0 },
	{ "compositeResult", 41, 0, 0, 0 },

	{ nullptr, 0, 0, 0, 0 },
};

CodeSamplerSource s_defaultCodeSamplers[] =
{
	{ "shadowmapSamplerSun", 6, 0, 0, 0 },
	{ "shadowmapSamplerSpot", 7, 0, 0, 0 },
	{ "feedbackSampler", 8, 0, 0, 0 },
	{ "floatZSampler", 19, 0, 0, 0 },
	{ "processedFloatZSampler", 20, 0, 0, 0 },
	{ "rawFloatZSampler", 21, 0, 0, 0 },
	{ "featherFloatZSampler", 28, 0, 0, 0 },
	{ "attenuationSampler", 16, 0, 0, 0 },
	{ "dlightAttenuationSampler", 17, 0, 0, 0 },
	{ "lightmapSamplerPrimary", 4, 0, 0, 0 },
	{ "lightmapSamplerSecondary", 5, 0, 0, 0 },
	{ "lightmapSamplerSecondaryB", 33, 0, 0, 0 },
	{ "modelLightingSampler", 3, 0, 0, 0 },
	{ "cinematicYSampler", 23, 0, 0, 0 },
	{ "cinematicCrSampler", 24, 0, 0, 0 },
	{ "cinematicCbSampler", 25, 0, 0, 0 },
	{ "cinematicASampler", 26, 0, 0, 0 },
	{ "reflectionProbeSampler", 27, 0, 0, 0 },
	{ "terrainScorchTextureSampler0", 29, 0, 0, 0 },
	{ "terrainScorchTextureSampler1", 30, 0, 0, 0 },
	{ "terrainScorchTextureSampler2", 31, 0, 0, 0 },
	{ "terrainScorchTextureSampler3", 32, 0, 0, 0 },
	{ "impactMaskSampler", 38, 0, 0, 0 },
	{ "ui3dSampler", 39, 0, 0, 0 },
	{ "missileCamSampler", 40, 0, 0, 0 },
	{ "heatmapSampler", 42, 0, 0, 0 },

	{ nullptr, 0, 0, 0, 0 },
};

CodeConstantSource s_nearPlaneConsts[] =
{
	{ "org", 16, 0, 0, 0 },
	{ "dx", 17, 0, 0, 0 },
	{ "dy", 18, 0, 0, 0 },

	{ nullptr, 0, 0, 0, 0 },
};

CodeConstantSource s_sunConsts[] =
{
	{ "position", 50, 0, 0, 0 },
	{ "diffuse", 51, 0, 0, 0 },
	{ "specular", 52, 0, 0, 0 },

	{ nullptr, 0, 0, 0, 0 },
};

CodeConstantSource s_lightConsts[] =
{
	{ "position", 0, 0, 0, 0 },
	{ "diffuse", 1, 0, 0, 0 },
	{ "specular", 2, 0, 0, 0 },
	{ "spotDir", 3, 0, 0, 0 },
	{ "spotFactors", 4, 0, 0, 0 },
	{ "falloffPlacement", 25, 0, 0, 0 },
	{ "attenuation", 5, 0, 0, 0 },
	{ "fallOffA", 6, 0, 0, 0 },
	{ "fallOffB", 7, 0, 0, 0 },
	{ "spotMatrix0", 8, 0, 0, 0 },
	{ "spotMatrix1", 9, 0, 0, 0 },
	{ "spotMatrix2", 10, 0, 0, 0 },
	{ "spotMatrix3", 11, 0, 0, 0 },
	{ "spotAABB", 12, 0, 0, 0 },
	{ "coneControl1", 13, 0, 0, 0 },
	{ "coneControl2", 14, 0, 0, 0 },
	{ "spotCookieSlideControl", 15, 0, 0, 0 },

	{ nullptr, 0, 0, 0, 0 },
};

CodeConstantSource s_codeConsts[] =
{
	{ "nearPlane", 230, s_nearPlaneConsts, 0, 0 },
	{ "sun", 230, s_sunConsts, 0, 0 },
	{ "light", 230, s_lightConsts, 0, 0 },

	{ "baseLightingCoords", 78, 0, 0, 0 },
	{ "lightingLookupScale", 53, 0, 0, 0 },
	{ "debugBumpmap", 54, 0, 0, 0 },
	{ "pixelCostFracs", 34, 0, 0, 0 },
	{ "pixelCostDecode", 35, 0, 0, 0 },
	{ "materialColor", 55, 0, 0, 0 },
	{ "fogConsts", 56, 0, 0, 0 },
	{ "fogConsts2", 57, 0, 0, 0 },
	{ "fogColor", 58, 0, 0, 0 },
	{ "sunFogColor", 61, 0, 0, 0 },
	{ "sunFogDir", 60, 0, 0, 0 },
	{ "sunFog", 59, 0, 0, 0 },
	{ "glowSetup", 62, 0, 0, 0 },
	{ "glowApply", 63, 0, 0, 0 },
	{ "filterTap", 36, 0, 8, 1 },
	{ "codeMeshArg", 76, 0, 2, 1 },
	{ "renderTargetSize", 21, 0, 0, 0 },
	{ "vposx_to_world", 22, 0, 0, 0 },
	{ "vposy_to_world", 23, 0, 0, 0 },
	{ "vpos1_to_world", 24, 0, 0, 0 },
	{ "shadowmapSwitchPartition", 47, 0, 0, 0 },
	{ "shadowmapScale", 48, 0, 0, 0 },
	{ "shadowmapPolygonOffset", 20, 0, 0, 0 },
	{ "shadowParms", 19, 0, 0, 0 },
	{ "zNear", 49, 0, 0, 0 },
	{ "clipSpaceLookupScale", 72, 0, 0, 0 },
	{ "clipSpaceLookupOffset", 73, 0, 0, 0 },
	{ "dofEquationViewModelAndFarBlur", 26, 0, 0, 0 },
	{ "dofEquationScene", 27, 0, 0, 0 },
	{ "dofLerpScale", 28, 0, 0, 0 },
	{ "dofLerpBias", 29, 0, 0, 0 },
	{ "dofRowDelta", 30, 0, 0, 0 },
	{ "depthFromClip", 75, 0, 0, 0 },
	{ "outdoorFeatherParms", 67, 0, 0, 0 },
	{ "skyTransition", 68, 0, 0, 0 },
	{ "envMapParms", 69, 0, 0, 0 },
	{ "waterParms", 80, 0, 0, 0 },
	{ "colorMatrixR", 44, 0, 0, 0 },
	{ "colorMatrixG", 45, 0, 0, 0 },
	{ "colorMatrixB", 46, 0, 0, 0 },
	{ "colorBias", 64, 0, 0, 0 },
	{ "colorTintBase", 65, 0, 0, 0 },
	{ "colorTintDelta", 66, 0, 0, 0 },
	{ "gameTime", 32, 0, 0, 0 },
	{ "alphaFade", 33, 0, 0, 0 },
	{ "destructibleParms", 114, 0, 0, 0 },
	{ "particleCloudColor", 31, 0, 0, 0 },
	{ "particleCloudMatrix", 74, 0, 0, 0 },
	{ "worldMatrix", 197, 0, 0, 0 },
	{ "inverseWorldMatrix", 198, 0, 0, 0 },
	{ "transposeWorldMatrix", 199, 0, 0, 0 },
	{ "inverseTransposeWorldMatrix", 200, 0, 0, 0 },
	{ "viewMatrix", 201, 0, 0, 0 },
	{ "inverseViewMatrix", 202, 0, 0, 0 },
	{ "transposeViewMatrix", 203, 0, 0, 0 },
	{ "inverseTransposeViewMatrix", 204, 0, 0, 0 },
	{ "projectionMatrix", 205, 0, 0, 0 },
	{ "inverseProjectionMatrix", 206, 0, 0, 0 },
	{ "transposeProjectionMatrix", 207, 0, 0, 0 },
	{ "inverseTransposeProjectionMatrix", 208, 0, 0, 0 },
	{ "worldViewMatrix", 209, 0, 0, 0 },
	{ "inverseWorldViewMatrix", 210, 0, 0, 0 },
	{ "transposeWorldViewMatrix", 211, 0, 0, 0 },
	{ "inverseTransposeWorldViewMatrix", 212, 0, 0, 0 },
	{ "viewProjectionMatrix", 213, 0, 0, 0 },
	{ "inverseViewProjectionMatrix", 214, 0, 0, 0 },
	{ "transposeViewProjectionMatrix", 215, 0, 0, 0 },
	{ "inverseTransposeViewProjectionMatrix", 216, 0, 0, 0 },
	{ "worldViewProjectionMatrix", 217, 0, 0, 0 },
	{ "inverseWorldViewProjectionMatrix", 218, 0, 0, 0 },
	{ "transposeWorldViewProjectionMatrix", 219, 0, 0, 0 },
	{ "inverseTransposeWorldViewProjectionMatrix", 220, 0, 0, 0 },
	{ "shadowLookupMatrix", 221, 0, 0, 0 },
	{ "inverseShadowLookupMatrix", 222, 0, 0, 0 },
	{ "transposeShadowLookupMatrix", 223, 0, 0, 0 },
	{ "inverseTransposeShadowLookupMatrix", 224, 0, 0, 0 },
	{ "worldOutdoorLookupMatrix", 225, 0, 0, 0 },
	{ "inverseWorldOutdoorLookupMatrix", 226, 0, 0, 0 },
	{ "transposeWorldOutdoorLookupMatrix", 227, 0, 0, 0 },
	{ "inverseTransposeWorldOutdoorLookupMatrix", 228, 0, 0, 0 },
	{ "windDirection", 79, 0, 0, 0 },
	{ "variantWindSpring", 98, 0, 16, 1 },
	{ "u_customWindCenter", 192, 0, 0, 0 },
	{ "u_customWindSpring", 193, 0, 0, 0 },
	{ "grassParms", 81, 0, 0, 0 },
	{ "grassForce0", 82, 0, 0, 0 },
	{ "grassForce1", 83, 0, 0, 0 },
	{ "grassWindForce0", 84, 0, 0, 0 },
	{ "cloudWorldArea", 115, 0, 0, 0 },
	{ "waterScroll", 116, 0, 0, 0 },
	{ "motionblurDirectionAndMagnitude", 85, 0, 0, 0 },
	{ "flameDistortion", 86, 0, 0, 0 },
	{ "bloomScale", 87, 0, 0, 0 },
	{ "overlayTexCoord", 88, 0, 0, 0 },
	{ "colorBias1", 89, 0, 0, 0 },
	{ "colorTintBase1", 90, 0, 0, 0 },
	{ "colorTintDelta1", 91, 0, 0, 0 },
	{ "fadeEffect", 92, 0, 0, 0 },
	{ "viewportDimensions", 93, 0, 0, 0 },
	{ "framebufferRead", 94, 0, 0, 0 },
	{ "resizeParams1", 95, 0, 0, 0 },
	{ "resizeParams2", 96, 0, 0, 0 },
	{ "resizeParams3", 97, 0, 0, 0 },
	{ "crossFadeAlpha", 117, 0, 0, 0 },
	{ "__characterCharredAmount", 118, 0, 0, 0 },
	{ "treeCanopyParms", 119, 0, 0, 0 },
	{ "marksHitNormal", 120, 0, 0, 0 },
	{ "postFxControl0", 121, 0, 0, 0 },
	{ "postFxControl1", 122, 0, 0, 0 },
	{ "postFxControl2", 123, 0, 0, 0 },
	{ "postFxControl3", 124, 0, 0, 0 },
	{ "postFxControl4", 125, 0, 0, 0 },
	{ "postFxControl5", 126, 0, 0, 0 },
	{ "postFxControl6", 127, 0, 0, 0 },
	{ "postFxControl7", 128, 0, 0, 0 },
	{ "postFxControl8", 129, 0, 0, 0 },
	{ "postFxControl9", 130, 0, 0, 0 },
	{ "postFxControlA", 131, 0, 0, 0 },
	{ "postFxControlB", 132, 0, 0, 0 },
	{ "postFxControlC", 133, 0, 0, 0 },
	{ "postFxControlD", 134, 0, 0, 0 },
	{ "postFxControlE", 135, 0, 0, 0 },
	{ "postFxControlF", 136, 0, 0, 0 },
	{ "hdrControl0", 137, 0, 0, 0 },
	{ "hdrControl1", 138, 0, 0, 0 },
	{ "glightPosXs", 139, 0, 0, 0 },
	{ "glightPosYs", 140, 0, 0, 0 },
	{ "glightPosZs", 141, 0, 0, 0 },
	{ "glightFallOffs", 142, 0, 0, 0 },
	{ "glightReds", 143, 0, 0, 0 },
	{ "glightGreens", 144, 0, 0, 0 },
	{ "glightBlues", 145, 0, 0, 0 },
	{ "dlightPosition", 146, 0, 0, 0 },
	{ "dlightDiffuse", 147, 0, 0, 0 },
	{ "dlightSpecular", 148, 0, 0, 0 },
	{ "dlightAttenuation", 149, 0, 0, 0 },
	{ "dlightFallOff", 150, 0, 0, 0 },
	{ "dlightSpotMatrix0", 151, 0, 0, 0 },
	{ "dlightSpotMatrix1", 152, 0, 0, 0 },
	{ "dlightSpotMatrix2", 153, 0, 0, 0 },
	{ "dlightSpotMatrix3", 154, 0, 0, 0 },
	{ "dlightSpotDir", 155, 0, 0, 0 },
	{ "dlightSpotFactors", 156, 0, 0, 0 },
	{ "dlightShadowLookupMatrix0", 157, 0, 0, 0 },
	{ "dlightShadowLookupMatrix1", 158, 0, 0, 0 },
	{ "dlightShadowLookupMatrix2", 159, 0, 0, 0 },
	{ "dlightShadowLookupMatrix3", 160, 0, 0, 0 },
	{ "cloudLayerControl0", 161, 0, 0, 0 },
	{ "cloudLayerControl1", 162, 0, 0, 0 },
	{ "cloudLayerControl2", 163, 0, 0, 0 },
	{ "cloudLayerControl3", 164, 0, 0, 0 },
	{ "cloudLayerControl4", 165, 0, 0, 0 },
	{ "heroLightingR", 166, 0, 0, 0 },
	{ "heroLightingG", 167, 0, 0, 0 },
	{ "heroLightingB", 168, 0, 0, 0 },
	{ "lightHeroScale", 169, 0, 0, 0 },
	{ "cinematicBlurBox", 170, 0, 0, 0 },
	{ "cinematicBlurBox2", 171, 0, 0, 0 },
	{ "adsZScale", 172, 0, 0, 0 },
	{ "ui3dUVSetup0", 173, 0, 0, 0 },
	{ "ui3dUVSetup1", 174, 0, 0, 0 },
	{ "ui3dUVSetup2", 175, 0, 0, 0 },
	{ "ui3dUVSetup3", 176, 0, 0, 0 },
	{ "ui3dUVSetup4", 177, 0, 0, 0 },
	{ "ui3dUVSetup5", 178, 0, 0, 0 },
	{ "__characterDissolveColor", 179, 0, 0, 0 },
	{ "cameraLook", 180, 0, 0, 0 },
	{ "cameraUp", 181, 0, 0, 0 },
	{ "cameraSide", 182, 0, 0, 0 },
	{ "scriptVector0", 183, 0, 0, 0 },
	{ "scriptVector1", 184, 0, 0, 0 },
	{ "scriptVector2", 185, 0, 0, 0 },
	{ "scriptVector3", 186, 0, 0, 0 },
	{ "scriptVector4", 187, 0, 0, 0 },
	{ "scriptVector5", 188, 0, 0, 0 },
	{ "scriptVector6", 189, 0, 0, 0 },
	{ "scriptVector7", 190, 0, 0, 0 },
	{ "eyeOffset", 191, 0, 0, 0 },
	{ "skyColorMultiplier", 194, 0, 0, 0 },
	{ "extraCamParam", 195, 0, 0, 0 },
	{ "emblemLUTSelector", 196, 0, 0, 0 },

	{ nullptr, 0, 0, 0, 0 },
};

CodeConstantSource s_defaultCodeConsts[] =
{
	{ "nearPlaneOrg", 16, 0, 0, 0 },
	{ "nearPlaneDx", 17, 0, 0, 0 },
	{ "nearPlaneDy", 18, 0, 0, 0 },
	{ "sunPosition", 50, 0, 0, 0 },
	{ "sunDiffuse", 51, 0, 0, 0 },
	{ "sunSpecular", 52, 0, 0, 0 },
	{ "lightPosition", 0, 0, 0, 0 },
	{ "lightDiffuse", 1, 0, 0, 0 },
	{ "lightSpecular", 2, 0, 0, 0 },
	{ "lightSpotDir", 3, 0, 0, 0 },
	{ "lightSpotFactors", 4, 0, 0, 0 },
	{ "lightFalloffPlacement", 25, 0, 0, 0 },
	{ "lightAttenuation", 5, 0, 0, 0 },
	{ "lightFallOffA", 6, 0, 0, 0 },
	{ "lightFallOffB", 7, 0, 0, 0 },
	{ "lightSpotMatrix0", 8, 0, 0, 0 },
	{ "lightSpotMatrix1", 9, 0, 0, 0 },
	{ "lightSpotMatrix2", 10, 0, 0, 0 },
	{ "lightSpotMatrix3", 11, 0, 0, 0 },
	{ "lightSpotAABB", 12, 0, 0, 0 },
	{ "lightConeControl1", 13, 0, 0, 0 },
	{ "lightConeControl2", 14, 0, 0, 0 },
	{ "lightSpotCookieSlideControl", 15, 0, 0, 0 },
	{ "spotShadowmapPixelAdjust", 70, 0, 0, 0 },
	{ "dlightSpotShadowmapPixelAdjust", 71, 0, 0, 0 },

	{ nullptr, 0, 0, 0, 0 },
};