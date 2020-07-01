using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using UnityEngine;

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
};

public class SqMaterial
{
    [DllImport("SquallGraphics")]
    static extern int AddNativeTexture(int _texID, IntPtr _texture);

    [DllImport("SquallGraphics")]
    static extern int AddNativeSampler(TextureWrapMode _wrapModeU, TextureWrapMode _wrapModeV, TextureWrapMode _wrapModeW, int _anisoLevel);

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
    /// white tex
    /// </summary>
    Texture2D whiteTex;

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
        SetupTexAndSampler(_mat, "_MainTex", ref mc._DiffuseIndex, ref mc._SamplerIndex);
        SetupTexAndSampler(_mat, "_SpecGlossMap", ref mc._SpecularIndex, ref dummy);
        SetupTexAndSampler(_mat, "_OcclusionMap", ref mc._OcclusionIndex, ref dummy);
        SetupTexAndSampler(_mat, "_EmissionMap", ref mc._EmissionIndex, ref dummy);
        SetupTexAndSampler(_mat, "_BumpMap", ref mc._NormalIndex, ref dummy);
        SetupTexAndSampler(_mat, "_DetailMask", ref mc._DetailMaskIndex, ref dummy);
        SetupTexAndSampler(_mat, "_DetailAlbedoMap", ref mc._DetailAlbedoIndex, ref dummy);
        SetupTexAndSampler(_mat, "_DetailNormalMap", ref mc._DetailNormalIndex, ref dummy);

        mc._MainTex_ST = new Vector4(_mat.mainTextureScale.x, _mat.mainTextureScale.y, _mat.mainTextureOffset.x, _mat.mainTextureOffset.y);
        mc._DetailAlbedoMap_ST = new Vector4(_mat.GetTextureScale("_DetailAlbedoMap").x, _mat.GetTextureScale("_DetailAlbedoMap").y
            , _mat.GetTextureOffset("_DetailAlbedoMap").x, _mat.GetTextureOffset("_DetailAlbedoMap").y);
        if (_mat.HasProperty("_Cutoff"))
        {
            mc._CutOff = _mat.GetFloat("_Cutoff");
        }

        // property
        mc._Color = _mat.color;
        mc._SpecColor = _mat.GetColor("_SpecColor");
        mc._Smoothness = _mat.GetFloat("_Glossiness");
        mc._OcclusionStrength = _mat.GetFloat("_OcclusionStrength");
        mc._EmissionColor = _mat.GetColor("_EmissionColor").linear;
        mc._BumpScale = _mat.GetFloat("_BumpScale");
        mc._DetailBumpScale = _mat.GetFloat("_DetailNormalMapScale");
        mc._DetailUV = _mat.GetFloat("_UVSec");

        return mc;
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

    void SetupTexAndSampler(Material _mat, string _texName, ref int _texIndex, ref int _samplerIndex)
    {
        Texture2D _tex = null;

        if (_mat.HasProperty(_texName))
        {
            _tex = _mat.GetTexture(_texName) as Texture2D;
        }

        if (_tex)
        {
            _texIndex = AddNativeTexture(_tex.GetInstanceID(), _tex.GetNativeTexturePtr());
            _samplerIndex = AddNativeSampler(_tex.wrapModeU, _tex.wrapModeV, _tex.wrapModeW, SqGraphicManager.instance.globalAnisoLevel);
        }
        else
        {
            _texIndex = AddNativeTexture(whiteTex.GetInstanceID(), whiteTex.GetNativeTexturePtr());
            _samplerIndex = AddNativeSampler(whiteTex.wrapModeU, whiteTex.wrapModeV, whiteTex.wrapModeW, SqGraphicManager.instance.globalAnisoLevel);
        }
    }

    void Init()
    {
        matConstantSize = Marshal.SizeOf(typeof(MaterialConstant));
        whiteTex = new Texture2D(1, 1);
        whiteTex.name = "Sq Default White";
        whiteTex.SetPixel(0, 0, Color.white);
        whiteTex.Apply();
    }

    void Release()
    {
        if (whiteTex)
        {
            GameObject.DestroyImmediate(whiteTex);
        }
    }
}
