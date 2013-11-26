﻿namespace msdk_analyzer
{
    partial class SdkAnalyzerForm
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.label1 = new System.Windows.Forms.Label();
            this.tbxLogOutput = new System.Windows.Forms.TextBox();
            this.checkBox_PerFrame = new System.Windows.Forms.CheckBox();
            this.button_Open = new System.Windows.Forms.Button();
            this.button_Start = new System.Windows.Forms.Button();
            this.label2 = new System.Windows.Forms.Label();
            this.lblBytesWritten = new System.Windows.Forms.Label();
            this.checkBox_Append = new System.Windows.Forms.CheckBox();
            this.SuspendLayout();
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(18, 27);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(58, 13);
            this.label1.TabIndex = 11;
            this.label1.Text = "Output File";
            // 
            // tbxLogOutput
            // 
            this.tbxLogOutput.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
                        | System.Windows.Forms.AnchorStyles.Right)));
            this.tbxLogOutput.Location = new System.Drawing.Point(99, 24);
            this.tbxLogOutput.Name = "tbxLogOutput";
            this.tbxLogOutput.Size = new System.Drawing.Size(390, 20);
            this.tbxLogOutput.TabIndex = 10;
            this.tbxLogOutput.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.tbxLogOutput_KeyPress);
            this.tbxLogOutput.Leave += new System.EventHandler(this.tbxLogOutput_Leave);
            // 
            // checkBox_PerFrame
            // 
            this.checkBox_PerFrame.AutoSize = true;
            this.checkBox_PerFrame.Location = new System.Drawing.Point(82, 116);
            this.checkBox_PerFrame.Name = "checkBox_PerFrame";
            this.checkBox_PerFrame.Size = new System.Drawing.Size(108, 17);
            this.checkBox_PerFrame.TabIndex = 15;
            this.checkBox_PerFrame.Text = "Per-frame logging";
            this.checkBox_PerFrame.UseVisualStyleBackColor = true;
            this.checkBox_PerFrame.CheckedChanged += new System.EventHandler(this.checkBox_PerFrame_CheckedChanged);
            // 
            // button_Open
            // 
            this.button_Open.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.button_Open.DialogResult = System.Windows.Forms.DialogResult.No;
            this.button_Open.Location = new System.Drawing.Point(435, 109);
            this.button_Open.Margin = new System.Windows.Forms.Padding(2);
            this.button_Open.Name = "button_Open";
            this.button_Open.Size = new System.Drawing.Size(56, 28);
            this.button_Open.TabIndex = 13;
            this.button_Open.Text = "Open";
            this.button_Open.UseVisualStyleBackColor = true;
            this.button_Open.Click += new System.EventHandler(this.button_Open_Click);
            // 
            // button_Start
            // 
            this.button_Start.Location = new System.Drawing.Point(21, 109);
            this.button_Start.Margin = new System.Windows.Forms.Padding(2);
            this.button_Start.Name = "button_Start";
            this.button_Start.Size = new System.Drawing.Size(56, 28);
            this.button_Start.TabIndex = 1;
            this.button_Start.Text = "Start";
            this.button_Start.UseVisualStyleBackColor = true;
            this.button_Start.Click += new System.EventHandler(this.button_Start_Click);
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(19, 51);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(77, 13);
            this.label2.TabIndex = 11;
            this.label2.Text = "KBytes Written";
            // 
            // lblBytesWritten
            // 
            this.lblBytesWritten.Location = new System.Drawing.Point(99, 51);
            this.lblBytesWritten.Name = "lblBytesWritten";
            this.lblBytesWritten.Size = new System.Drawing.Size(91, 13);
            this.lblBytesWritten.TabIndex = 16;
            this.lblBytesWritten.Text = "0";
            // 
            // checkBox_Append
            // 
            this.checkBox_Append.AutoSize = true;
            this.checkBox_Append.Location = new System.Drawing.Point(196, 116);
            this.checkBox_Append.Name = "checkBox_Append";
            this.checkBox_Append.Size = new System.Drawing.Size(129, 17);
            this.checkBox_Append.TabIndex = 13;
            this.checkBox_Append.Text = "Append to existing file";
            this.checkBox_Append.UseVisualStyleBackColor = true;
            // 
            // SdkAnalyzerForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(501, 148);
            this.Controls.Add(this.checkBox_Append);
            this.Controls.Add(this.lblBytesWritten);
            this.Controls.Add(this.button_Start);
            this.Controls.Add(this.checkBox_PerFrame);
            this.Controls.Add(this.button_Open);
            this.Controls.Add(this.label2);
            this.Controls.Add(this.label1);
            this.Controls.Add(this.tbxLogOutput);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.MaximizeBox = false;
            this.Name = "SdkAnalyzerForm";
            this.ShowIcon = false;
            this.Text = "Intel Media SDK Tracer";
            this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.Form4_FormClosing);
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.TextBox tbxLogOutput;
        private System.Windows.Forms.CheckBox checkBox_PerFrame;        private System.Windows.Forms.Button button_Open;
        private System.Windows.Forms.Button button_Start;
        private System.Windows.Forms.CheckBox checkBox_Append;        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.Label lblBytesWritten;
    }
}