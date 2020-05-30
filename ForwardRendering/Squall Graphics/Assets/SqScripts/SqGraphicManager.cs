using System.Runtime.InteropServices;
using UnityEngine;

/// <summary>
/// squall graphic manager
/// </summary>
public class SqGraphicManager : MonoBehaviour
{
    [StructLayout(LayoutKind.Sequential)]
    struct GameTime
    {
        /// <summary>
        /// update time
        /// </summary>
        public double updateTime;

        /// <summary>
        /// render time
        /// </summary>
        public double renderTime;

        /// <summary>
        /// gpu time
        /// </summary>
        public double gpuTime;

        /// <summary>
        /// culling time
        /// </summary>
        public double cullingTime;

        /// <summary>
        /// sorting time
        /// </summary>
        public double sortingTime;

        /// <summary>
        /// batch count
        /// </summary>
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)]
        public int[] batchCount;

        /// <summary>
        /// render thread time
        /// </summary>
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)]
        public double[] renderThreadTime;
    };

    [DllImport("SquallGraphics")]
    static extern bool InitializeSqGraphic(int _numOfThreads);

    [DllImport("SquallGraphics")]
    static extern void ReleaseSqGraphic();

    [DllImport("SquallGraphics")]
    static extern void UpdateSqGraphic();

    [DllImport("SquallGraphics")]
    static extern void RenderSqGraphic();

    [DllImport("SquallGraphics")]
    static extern int GetRenderThreadCount();

    [DllImport("SquallGraphics")]
    static extern GameTime GetGameTime();

    /// <summary>
    /// instance
    /// </summary>
    public static SqGraphicManager instance;

    /// <summary>
    /// number of render threads
    /// </summary>
    [Range(2, 32)]
    public int numOfRenderThreads = 4;

    /// <summary>
    /// global aniso level
    /// </summary>
    public int globalAnisoLevel = 8;

    /// <summary>
    /// print timer info
    /// </summary>
    public bool printTimerInfo = false;

    GameTime gameTime;
    float gameTimeUpdate = 0f;
    GUIStyle guiStyle;
    Texture2D guiBg;

    void Awake()
    {
        instance = this;

        if (numOfRenderThreads < 2)
        {
            numOfRenderThreads = 2;
        }

        if (InitializeSqGraphic(numOfRenderThreads))
        {
            Debug.Log("[SqGraphicManager] Squall Graphics initialized.");
        }
        else
        {
            Debug.LogError("[SqGraphicManager] Squall Graphics terminated.");
            enabled = false;
            return;
        }

        Debug.Log("[SqGraphicManager] Number of render threads: " + GetRenderThreadCount());
        guiStyle = new GUIStyle();
        guiStyle.fontSize = 24 * Screen.width / 1920;
        guiStyle.normal.textColor = Color.white;

#if DEBUG
        guiBg = new Texture2D(1, 1);
        guiBg.SetPixel(0, 0, new Color(0, 0, 0, 0.5f));
        guiBg.Apply();
#endif
    }

    void OnDestroy()
    {
        ReleaseSqGraphic();

#if DEBUG
        DestroyImmediate(guiBg);
#endif
    }

    void LateUpdate()
    {
        UpdateSqGraphic();
        RenderSqGraphic();
        gameTimeUpdate += Time.deltaTime;
    }

#if DEBUG
    void OnGUI()
    {
        if (printTimerInfo)
        {
            if (gameTimeUpdate > 1f)
            {
                gameTime = GetGameTime();
                gameTimeUpdate = 0f;
            }

            GUI.DrawTexture(new Rect(0, 0, 500, 140 + (numOfRenderThreads - 1) * 20), guiBg, ScaleMode.StretchToFill, true);

            GUI.Label(new Rect(10, 10, 500, 20 * Screen.width / 1920), "Update() Call Time: " + gameTime.updateTime.ToString("0.#####") + " ms", guiStyle);
            GUI.Label(new Rect(10, 10 + 20 * Screen.width / 1920, 500, 20 * Screen.width / 1920), "Render() Call Time: " + gameTime.renderTime.ToString("0.#####") + " ms", guiStyle);
            GUI.Label(new Rect(10, 10 + 40 * Screen.width / 1920, 500, 20 * Screen.width / 1920), "GPU Time: " + gameTime.gpuTime.ToString("0.#####") + " ms", guiStyle);
            GUI.Label(new Rect(10, 10 + 60 * Screen.width / 1920, 500, 20 * Screen.width / 1920), "Culling Time: " + gameTime.cullingTime.ToString("0.#####") + " ms", guiStyle);
            GUI.Label(new Rect(10, 10 + 80 * Screen.width / 1920, 500, 20 * Screen.width / 1920), "Sorting Time: " + gameTime.sortingTime.ToString("0.#####") + " ms", guiStyle);

            if (gameTime.batchCount != null)
            {
                int batchCount = 0;
                for (int i = 0; i < numOfRenderThreads - 1; i++)
                {
                    batchCount += gameTime.batchCount[i];
                }
                GUI.Label(new Rect(10, 10 + 100 * Screen.width / 1920, 500, 20 * Screen.width / 1920), "Batch Count: " + batchCount.ToString(), guiStyle);
            }

            if (gameTime.renderThreadTime != null)
            {
                for (int i = 0; i < numOfRenderThreads - 1; i++)
                {
                    if (i < gameTime.renderThreadTime.Length)
                    {
                        GUI.Label(new Rect(10, 10 + (120 + 20 * i) * Screen.width / 1920, 500, 20 * Screen.width / 1920), "RenderThread " + i + " Time: " + gameTime.renderThreadTime[i].ToString("0.#####") + " ms", guiStyle);
                    }
                }
            }
        }
    }
#endif
}
