namespace Ui
{
    partial class MainForm
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
            this.MessageOutput = new System.Windows.Forms.RichTextBox();
            this.CommandInput = new System.Windows.Forms.TextBox();
            this.reloadButton = new System.Windows.Forms.Button();
            this.flowLayoutPanel1 = new System.Windows.Forms.FlowLayoutPanel();
            this.SuspendLayout();
            // 
            // MessageOutput
            // 
            this.MessageOutput.BackColor = System.Drawing.Color.Black;
            this.MessageOutput.ForeColor = System.Drawing.Color.White;
            this.MessageOutput.Location = new System.Drawing.Point(0, 0);
            this.MessageOutput.Name = "MessageOutput";
            this.MessageOutput.ReadOnly = true;
            this.MessageOutput.Size = new System.Drawing.Size(737, 220);
            this.MessageOutput.TabIndex = 0;
            this.MessageOutput.Text = "";
            // 
            // CommandInput
            // 
            this.CommandInput.Location = new System.Drawing.Point(0, 226);
            this.CommandInput.Name = "CommandInput";
            this.CommandInput.Size = new System.Drawing.Size(735, 20);
            this.CommandInput.TabIndex = 1;
            this.CommandInput.KeyDown += new System.Windows.Forms.KeyEventHandler(this.CommandInput_KeyDown);
            // 
            // reloadButton
            // 
            this.reloadButton.Location = new System.Drawing.Point(673, 354);
            this.reloadButton.Name = "reloadButton";
            this.reloadButton.Size = new System.Drawing.Size(75, 23);
            this.reloadButton.TabIndex = 6;
            this.reloadButton.Text = "Reload";
            this.reloadButton.UseVisualStyleBackColor = true;
            this.reloadButton.Click += new System.EventHandler(this.reloadButton_Click);
            // 
            // flowLayoutPanel1
            // 
            this.flowLayoutPanel1.Location = new System.Drawing.Point(13, 253);
            this.flowLayoutPanel1.Name = "flowLayoutPanel1";
            this.flowLayoutPanel1.Size = new System.Drawing.Size(722, 100);
            this.flowLayoutPanel1.TabIndex = 8;
            // 
            // MainForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(747, 378);
            this.Controls.Add(this.flowLayoutPanel1);
            this.Controls.Add(this.reloadButton);
            this.Controls.Add(this.CommandInput);
            this.Controls.Add(this.MessageOutput);
            this.Name = "MainForm";
            this.Text = "Dlundotch Tool";
            this.FormClosed += new System.Windows.Forms.FormClosedEventHandler(this.MainForm_FormClosed);
            this.Load += new System.EventHandler(this.MainForm_Load);
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.RichTextBox MessageOutput;
        private System.Windows.Forms.TextBox CommandInput;
        private System.Windows.Forms.Button reloadButton;
        private System.Windows.Forms.FlowLayoutPanel flowLayoutPanel1;
    }
}

