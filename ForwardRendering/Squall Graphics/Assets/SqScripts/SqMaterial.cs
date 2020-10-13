using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Rendering;

/// <summary>
/// material constant
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct MaterialConstant
{
    public Vector4 _MainTex_ST;
    public Vector4 _DetailAlbedoMap_ST;
    public Vector4 _Color;
    public Vector4 _SpecColor;
    public Vector4 _EmissionColor;
    public float _CutOff;
    public float _Smoothness;
    public float _OcclusionStrength;
    public float _BumpScale;
    public float _DetailBumpScale;
    public float _DetailUV;
    public int _DiffuseIndex;
    public int _SamplerIndex;
    public int _SpecularIndex;
    public int _OcclusionIndex;
    public int _EmissionIndex;
    public int _NormalIndex;
    public int _DetailMaskIndex;
    public int _DetailAlbedoIndex;
    public int _DetailNormalIndex;
    public int _ReflectionCount;
    public int _RenderQueue;
};

public class SqMaterial
{
    [DllImport("SquallGraphics")]
    static extern int AddNativeTexture(int _texID, IntPtr _texture);

    [DllImport("SquallGraphics")]
    static extern int AddNativeSampler(TextureWrapMode _wrapModeU, TextureWrapMode _wrapModeV, TextureWrapMode _wrapModeW, int _anisoLevel);

    [DllImport("SquallGraphics")]
    static extern void UpdateNativeMaterialProp(int _instanceID, uint _byteSize, MaterialConstant _mc);

    [DllImport("SquallGraphics", CharSet = CharSet.Ansi)]
    static extern int AddNativeMaterial(int _nRendererId, int _matInstanceId, int _queue, int _cullMode, int _srcBlend, int _dstBlend, string _nativeShader, int _numMacro, string[] _macro);

    /// <summary>
    /// instance
    /// </summary>
    private static SqMaterial instance = null;

    /// <summary>
    /// singleton instance
    /// </summary>
    public static SqMaterial Instance
    {
        get
        {
            if (instance == null)
            {
                instance = new SqMaterial();
                instance.Init();
            }
            return instance;
        }
    }

    private SqMaterial()
    {
        // prevent constructor
    }

    ~SqMaterial()
    {
        Release();
    }

    /// <summary>
    /// constant size
    /// </summary>
    public int matConstantSize;

    /// <summary>
    /// material cache
    /// </summary>
    Dictionary<int, Material> materialCache;

    /// <summary>
    /// white tex
    /// </summary>
    Texture2D whiteTex;

    Texture2D blackTex;

    /// <summary>
    /// get material constant
    /// </summary>
    /// <param name="_mat"> mat </param>
    /// <returns> material constant </returns>
    public MaterialConstant GetMaterialConstant(Material _mat)
    {
        MaterialConstant mc = new MaterialConstant();

        // get diffuse
        int dummy = 0;
        SetupTexAndSampler(_mat, "_MainTex", ref mc._DiffuseIndex, ref mc._SamplerIndex, whiteTex);
        SetupTexAndSampler(_mat, "_SpecGlossMap", ref mc._SpecularIndex, ref dummy, blackTex);
        SetupTexAndSampler(_mat, "_OcclusionMap", ref mc._OcclusionIndex, ref dummy, whiteTex);
        SetupTexAndSampler(_mat, "_EmissionMap", ref mc._EmissionIndex, ref dummy, blackTex);
        SetupTexAndSampler(_mat, "_BumpMap", ref mc._NormalIndex, ref dummy, blackTex);
        SetupTexAndSampler(_mat, "_DetailMask", ref mc._DetailMaskIndex, ref dummy, blackTex);
        SetupTexAndSampler(_mat, "_DetailAlbedoMap", ref mc._DetailAlbedoIndex, ref dummy, whiteTex);
        SetupTexAndSampler(_mat, "_DetailNormalMap", ref mc._DetailNormalIndex, ref dummy, blackTex);

        mc._MainTex_ST = new Vector4(_mat.mainTextureScale.x, _mat.mainTextureScale.y, _mat.mainTextureOffset.x, _mat.mainTextureOffset.y);
        mc._DetailAlbedoMap_ST = new Vector4(_mat.GetTextureScale("_DetailAlbedoMap").x, _mat.GetTextureScale("_DetailAlbedoMap").y
            , _mat.GetTextureOffset("_DetailAlbedoMap").x, _mat.GetTextureOffset("_DetailAlbedoMap").y);
        if (_mat.HasProperty("_Cutoff"))
        {
            mc._CutOff = _mat.GetFloat("_Cutoff");
            if (_mat.renderQueue < 2226)
            {
                mc._CutOff = 0;
            }
        }

        // property
        mc._Color = _mat.color.linear;
        mc._SpecColor = _mat.GetColor("_SpecColor").linear;
        mc._Smoothness = _mat.GetFloat("_Glossiness");
        mc._OcclusionStrength = _mat.GetFloat("_OcclusionStrength");
        mc._EmissionColor = _mat.GetColor("_EmissionColor").linear;
        mc._BumpScale = _mat.GetFloat("_BumpScale");
        mc._DetailBumpScale = _mat.GetFloat("_DetailNormalMapScale");
        mc._DetailUV = _mat.GetFloat("_UVSec");
        mc._ReflectionCount = _mat.GetInt("_ReflectionCount");

        bool isCutOff = (_mat.renderQueue <= (int)RenderQueue.GeometryLast) && (_mat.renderQueue >= 2226);
        bool isTransparent = (_mat.renderQueue > (int)RenderQueue.GeometryLast);

        if (isCutOff)
            mc._RenderQueue = 1;
        else if (isTransparent)
            mc._RenderQueue = 2;
        else
            mc._RenderQueue = 0;

        return mc;
    }

    public void ExtractMaterialData(Material _mat, int _rendererID)
    {
        int cullMode = (_mat.HasProperty("_CullMode")) ? (int)_mat.GetFloat("_CullMode") : 2;
        int srcBlend = (_mat.HasProperty("_SrcBlend")) ? (int)_mat.GetFloat("_SrcBlend") : 1;
        int dstBlend = (_mat.HasProperty("_DstBlend")) ? (int)_mat.GetFloat("_DstBlend") : 0;
        bool isCutOff = (_mat.renderQueue <= (int)RenderQueue.GeometryLast) && (_mat.renderQueue >= 2226);
        bool isTransparent = (_mat.renderQueue > (int)RenderQueue.GeometryLast);

        List<string> macro = new List<string>();

        if (isCutOff)
        {
            macro.Add("_CUTOFF_ON");
        }

        if (isTransparent)
        {
            macro.Add("_TRANSPARENT_ON");
        }

        AddTexKeyword(_mat, ref macro, "_SpecGlossMap", "_SPEC_GLOSS_MAP");
        AddTexKeyword(_mat, ref macro, "_BumpMap", "_NORMAL_MAP");
        AddTexKeyword(_mat, ref macro, "_DetailAlbedoMap", "_DETAIL_MAP");
        AddTexKeyword(_mat, ref macro, "_DetailNormalMap", "_DETAIL_NORMAL_MAP");

        bool shouldEmissionBeEnabled = (_mat.globalIlluminationFlags & MaterialGlobalIlluminationFlags.EmissiveIsBlack) == 0;
        if (shouldEmissionBeEnabled)
        {
            macro.Add("_EMISSION");
        }

        if (_mat.GetFloat("_UseFresnel") > 0)
        {
            macro.Add("_FRESNEL_EFFECT");
        }

        AddNativeMaterial(_rendererID, _mat.GetInstanceID(), _mat.renderQueue, cullMode, srcBlend, dstBlend, "ForwardPass.hlsl", macro.Count, macro.ToArray());
        macro.Clear();

        // cache and update props
        if (!HasMaterial(_mat))
        {
            MaterialConstant mc = GetMaterialConstant(_mat);
            UpdateNativeMaterialProp(_mat.GetInstanceID(), (uint)matConstantSize, mc);
            CacheMaterial(_mat, mc);
        }
    }

    public void AddTexKeyword(Material _mat, ref List<string> _macro, string _texName, string _keyName)
    {
        if (_mat.HasProperty(_texName))
        {
            if (_mat.GetTexture(_texName))
            {
                _macro.Add(_keyName);
            }
        }
    }

    public void CacheMaterial(Material _mat, MaterialConstant _mc)
    {
        int id = _mat.GetInstanceID();
        if (!materialCache.ContainsKey(id))
        {
            materialCache.Add(id, _mat);
        }
    }

    public bool HasMaterial(Material _mat)
    {
        return materialCache.ContainsKey(_mat.GetInstanceID());
    }

    public void UpdateMaterial(Material _mat)
    {
        int id = _mat.GetInstanceID();
        if (!materialCache.ContainsKey(id))
        {
            return;
        }

        var mc = GetMaterialConstant(_mat);
        UpdateNativeMaterialProp(id, (uint)matConstantSize, mc);
    }

    void SetupTexAndSampler(Material _mat, string _texName, ref int _texIndex, ref int _samplerIndex, Texture2D _fallbackTex)
    {
        Texture2D _tex = null;

        if (_mat.HasProperty(_texName))
        {
            _tex = _mat.GetTexture(_texName) as Texture2D;
        }

        if (_tex)
        {
            _texIndex = AddNativeTexture(_tex.GetInstanceID(), _tex.GetNativeTexturePtr());
            _samplerIndex = AddNativeSampler(_tex.wrapModeU, _tex.wrapModeV, _tex.wrapModeW, SqGraphicManager.Instance.globalAnisoLevel);
        }
        else
        {
            _texIndex = AddNativeTexture(_fallbackTex.GetInstanceID(), _fallbackTex.GetNativeTexturePtr());
            _samplerIndex = AddNativeSampler(_fallbackTex.wrapModeU, _fallbackTex.wrapModeV, _fallbackTex.wrapModeW, SqGraphicManager.Instance.globalAnisoLevel);
        }
    }

    void Init()
    {
        matConstantSize = Marshal.SizeOf(typeof(MaterialConstant));

        whiteTex = new Texture2D(1, 1);
        whiteTex.name = "Sq Default White";
        whiteTex.SetPixel(0, 0, Color.white);
        whiteTex.Apply();

        blackTex = new Texture2D(1, 1);
        blackTex.name = "Sq Default Black";
        blackTex.SetPixel(0, 0, Color.clear);
        blackTex.Apply();

        materialCache = new Dictionary<int, Material>();
    }

    void Release()
    {
        if (whiteTex)
        {
            GameObject.DestroyImmediate(whiteTex);
        }

        if(blackTex)
        {
            GameObject.DestroyImmediate(blackTex);
        }

        materialCache.Clear();
    }
}
